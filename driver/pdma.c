#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <mach/a2f.h>

#include "pdma.h"

#define PDMA_REGS 0x40004000 
#define PDMA_IRQ  9

/*
 * Spinlock for accessing the PDMA regs
 */
static spinlock_t pdma_lock;

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int pdma_debug = 0;

/*
 * User can change verbosity of the driver
 */
module_param(pdma_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(pdma_debug, "pdma debugging level, >0 is verbose");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (pdma_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

/*
 * Peripheral DMA resources.
 */
static struct resource pdma_dev0_resources[] = {
	{
        .start	= PDMA_REGS,
        .end	= PDMA_REGS + 1,
        .flags	= IORESOURCE_MEM,
	}, {
        .start  = PDMA_IRQ,
        .flags  = IORESOURCE_IRQ,
	},
};

/*
 * Peripheral DMA platform device.
 * Instantiates the driver.
 */
static struct platform_device pdma_dev0 = {
	.name           = "pdma",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(pdma_dev0_resources),
	.resource       = pdma_dev0_resources,
};

/*
 * Bookkeeping structure for each DMA channel.
 */
struct pdma_channel_t {
    u8 channel;
    u8 in_use;
    u32 flags;
    u8 write_adj;
    pdma_irq_handler_t handler;
    void* user_data;
    struct pdma_channel_buffer_status_t {
        u32 src;
        u32 dst;
        u16 cnt;
    } buf[2]; // Buffer A & B
};

/*
 * Periperhal DMA device data structure.
 */
struct pdma_device_t {
    void* regs;
    int irq;
    struct pdma_channel_t channel[8];
} *pdma_device;

/*
 * Peripheral DMA registers, for ioremap.
 */
struct pdma_regs_t {
	u32	ratio;
	u32	status;
	u32	reserved[(0x20 - 0x08) >> 2];
	struct pdma_chan_t {
		u32	control;
		u32	status;
		struct {
			u32	src;
			u32	dst;
			u32	cnt;
		} buf[2];			/* Buffers A-B	*/
	} chan[8];				/* Channels 0-7	*/
};

#define PDMA(s) ((volatile struct pdma_regs_t *)(s->regs))

static int d_printk_pdma_ch(int level, int channel) {
    d_printk(level, "ratio: %08x\n", PDMA(pdma_device)->ratio);
    d_printk(level, "status: %08x\n", PDMA(pdma_device)->status);
    d_printk(level, "channel control: %08x\n", PDMA(pdma_device)->chan[channel].control);
    d_printk(level, "channel status: %08x\n", PDMA(pdma_device)->chan[channel].status);
    d_printk(level, "channel buf A src: %08x\n", PDMA(pdma_device)->chan[channel].buf[0].src);
    d_printk(level, "channel buf A dst: %08x\n", PDMA(pdma_device)->chan[channel].buf[0].dst);
    d_printk(level, "channel buf A cnt: %d\n", PDMA(pdma_device)->chan[channel].buf[0].cnt);
    d_printk(level, "channel buf B src: %08x\n", PDMA(pdma_device)->chan[channel].buf[1].src);
    d_printk(level, "channel buf B dst: %08x\n", PDMA(pdma_device)->chan[channel].buf[1].dst);
    d_printk(level, "channel buf B cnt: %d\n", PDMA(pdma_device)->chan[channel].buf[1].cnt);
    return 0;
}

int pdma_set_priority_ratio(int ratio) {
    PDMA(pdma_device)->ratio = ratio;
    return 0;
}
EXPORT_SYMBOL(pdma_set_priority_ratio);

int pdma_start(u8 ch,
               u32 src,
               u32 dst,
               u16 cnt) {
    unsigned long flags = 0;
    int ret = 0;
    int buf = -EINVAL;
    struct pdma_channel_t* channel;

    if (ch >= 8) {
        d_printk(0, "Invalid channel number %d\n", ch);
        ret = -EINVAL;
        goto done_pdma_start;
    }

    spin_lock_irqsave(&pdma_lock, flags);

    channel = &pdma_device->channel[ch];
    if (!channel->in_use) {
        d_printk(0, "Channel %d must be requested first\n", ch);
        ret = -EINVAL;
        goto done_pdma_start;
    }

    buf = !!(PDMA(pdma_device)->chan[ch].status & PDMA_STATUS_BUF_SEL);

    d_printk(2, "pdma_start ch=%d buf=%c src=%08x dst=%08x cnt=%d\n",
        ch, buf ? 'B' : 'A', src, dst, cnt);

    channel->buf[buf].src = src;
    channel->buf[buf].dst = dst;
    channel->buf[buf].cnt = cnt;

    d_printk_pdma_ch(4, ch);

    PDMA(pdma_device)->chan[ch].buf[buf].src = src;
    PDMA(pdma_device)->chan[ch].buf[buf].dst = dst;
    PDMA(pdma_device)->chan[ch].buf[buf].cnt = cnt;

    d_printk_pdma_ch(4, ch);

done_pdma_start:
    spin_unlock_irqrestore(&pdma_lock, flags);
    return ret;
} EXPORT_SYMBOL(pdma_start);

int pdma_busy(u8 ch) {
    // TODO: There must be a cleaner way to check this.
    return PDMA(pdma_device)->chan[ch].buf[0].cnt || PDMA(pdma_device)->chan[ch].buf[1].cnt;
} EXPORT_SYMBOL(pdma_busy);

int pdma_request(u8 ch, pdma_irq_handler_t handler, void* user_data, u8 write_adj, u32 flags) {
    unsigned long irqflags = 0;
    int ret = 0;
    struct pdma_channel_t* channel;
    
    if (ch >= 8) {
        d_printk(0, "Invalid channel number %d\n", ch);
        ret = -EINVAL;
        goto fail_args;
    }

    spin_lock_irqsave(&pdma_lock, irqflags);
    channel = &pdma_device->channel[ch];
    if (channel->in_use) {
        d_printk(0, "Channel already in use %d\n", ch);
        ret = -EBUSY;
        goto fail_args;
    }

    d_printk(2, "configuring channel %d\n", ch);

    channel->in_use = 1;
    channel->flags = flags;
    channel->write_adj = write_adj;
    channel->handler = handler;
    channel->user_data = user_data;

    PDMA(pdma_device)->chan[ch].control = PDMA_CONTROL_RESET |
            PDMA_CONTROL_CLR_A |
            PDMA_CONTROL_CLR_B;

    PDMA(pdma_device)->chan[ch].control =
            ((u32)channel->write_adj << 14) | channel->flags;
fail_args:
    spin_unlock_irqrestore(&pdma_lock, irqflags);
    return ret;
} EXPORT_SYMBOL(pdma_request);

void pdma_release(u8 ch) {
    unsigned long flags;
    d_printk(2, "release channel %d\n", ch);

    spin_lock_irqsave(&pdma_lock, flags);

    pdma_device->channel[ch].in_use = 0;
    pdma_device->channel[ch].handler = 0;
    pdma_device->channel[ch].user_data = 0;
    pdma_device->channel[ch].buf[0].src = 0;
    pdma_device->channel[ch].buf[0].dst = 0;
    pdma_device->channel[ch].buf[0].cnt = 0;
    pdma_device->channel[ch].buf[1].src = 0;
    pdma_device->channel[ch].buf[1].dst = 0;
    pdma_device->channel[ch].buf[1].cnt = 0;
    PDMA(pdma_device)->chan[ch].control = PDMA_CONTROL_RESET |
            PDMA_CONTROL_CLR_A |
            PDMA_CONTROL_CLR_B;

    spin_unlock_irqrestore(&pdma_lock, flags);
} EXPORT_SYMBOL(pdma_release);

static int pdma_init(struct pdma_device_t* p) {
	uint32_t v;
    u8 ch;
    int ret = 0;
    d_printk(2, "init\n");

    spin_lock_init(&pdma_lock);
    /*
     * Reset the PDMA block.
     */
#define PERIPHERAL_DMA_SOFT_RESET (1<<5)
    v = readl(&A2F_SYSREG->soft_rst_cr) |
        PERIPHERAL_DMA_SOFT_RESET;
    writel(v, &A2F_SYSREG->soft_rst_cr);

    /*
     * Take PDMA controller out of reset.
     */
    v = readl(&A2F_SYSREG->soft_rst_cr) &
        ~PERIPHERAL_DMA_SOFT_RESET;
    writel(v, &A2F_SYSREG->soft_rst_cr);

    PDMA(p)->ratio = PDMA_RATIO_HIGH_LOW_255_TO_1;

    /*
     * Initialize the PDMA channel bookkeeping structure.
     */
    for (ch = 0; ch < 8; ++ch) {
        p->channel[ch].channel = ch;
        p->channel[ch].in_use = 0;
        p->channel[ch].handler = 0;
        p->channel[ch].user_data = 0;
        p->channel[ch].buf[0].src = 0;
        p->channel[ch].buf[0].dst = 0;
        p->channel[ch].buf[0].cnt = 0;
        p->channel[ch].buf[1].src = 0;
        p->channel[ch].buf[1].dst = 0;
        p->channel[ch].buf[1].cnt = 0;
    }

    return ret;
}

static irqreturn_t pdma_irq_cb(int irq, void* ptr) {
    struct pdma_device_t* p = ptr;
    u8 ch;
    u32 status;
    d_printk(2, "irq\n");

    status = PDMA(p)->status;
    for (ch = 0; ch < 8; ++ch) {
        if(status & (1 << (ch*2))) {
            d_printk(2, "channel %d buffer A done\n", ch);
            d_printk_pdma_ch(4, ch);
            PDMA(p)->chan[ch].control |= PDMA_CONTROL_CLR_A;
            d_printk_pdma_ch(4, ch);
        }
        if(status & (2 << (ch*2))) {
            d_printk(2, "channel %d buffer B done\n", ch);
            d_printk_pdma_ch(4, ch);
            PDMA(p)->chan[ch].control |= PDMA_CONTROL_CLR_B;
            d_printk_pdma_ch(4, ch);
        }
        if ((status & (3 << (ch*2)) && p->channel[ch].handler)) {
            p->channel[ch].handler(p->channel[ch].user_data);
        }
    }

	return IRQ_HANDLED;
}

static int pdma_probe(struct platform_device *pdev) {
    struct resource* pdma_regs;
    int irq;
	int ret = 0;

	pdma_regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!pdma_regs) {
		d_printk(0, "no register base for PDMA controller\n");
		ret = -ENXIO;
		goto error_release_nothing;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		d_printk(0, "invalid IRQ %d\n", irq);
		ret = -ENXIO;
		goto error_release_nothing;
	}

    pdma_device = kzalloc(sizeof(struct pdma_device_t), GFP_KERNEL);

	pdma_device->regs = ioremap(pdma_regs->start, resource_size(pdma_regs));
	if (!pdma_device->regs) {
		d_printk(0, "unable to map registers for "
			"PDMA controller base=%08x\n", pdma_regs->start);
		ret = -EINVAL;
		goto error_release_nothing;
	}

    pdma_device->irq = irq;

	ret = request_irq(irq, pdma_irq_cb, 0, dev_name(&pdev->dev), pdma_device);
	if (ret) {
		d_printk(0, "request irq %d failed for pdma", irq);
		ret = -EINVAL;
		goto error_release_pdma_regs;
	}

    if (pdma_init(pdma_device) != 0) {
        d_printk(0, "failed to init pdma");
        ret = -EACCES;
        goto error_release_irq;
    }

    goto done;

error_release_irq:
    free_irq(pdma_device->irq, pdma_device);
error_release_pdma_regs:
	iounmap(pdma_device->regs);
error_release_nothing:
done:
    return ret;
}

static int pdma_remove(struct platform_device *pdev) {
    d_printk(2, "pdma_remove\n");
    free_irq(pdma_device->irq, pdma_device);
    iounmap(pdma_device->regs);
    kfree(pdma_device);
    return 0;
}
/*
 * Platform driver data structure
 */
static struct platform_driver pdma_driver = {
	.probe  = pdma_probe,
	.remove = pdma_remove,
	.driver = {
		.name = "pdma",
		.owner = THIS_MODULE,
	},
};

/*
 * Driver init
 */
static int __init pdma_module_init(void) {
	platform_driver_register(&pdma_driver);
    platform_device_register(&pdma_dev0);
    return 0;
}

/*
 * Driver clean-up
 */
static void __exit pdma_module_exit(void) {
    platform_device_unregister(&pdma_dev0);
	platform_driver_unregister(&pdma_driver);
}

module_init(pdma_module_init);
module_exit(pdma_module_exit);
MODULE_AUTHOR("Chris Testa, <chris@testa.co>");
MODULE_DESCRIPTION("Device driver for PDMA on SmartFusion");
MODULE_LICENSE("GPL");
