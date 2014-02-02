#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <mach/a2f.h>

#include "pdma.h"

static int *STCVR = (int *)0xE000E018;

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

static struct dentry *pdma_debugfs_root;

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

#define PDMA_CHANNEL_BUFFER_A  0
#define PDMA_CHANNEL_BUFFER_B  1

#define PDMA_CHANNEL_BUFFER_STOPPED   0
#define PDMA_CHANNEL_BUFFER_STARTED   1

#define PDMA_CHANNEL_NEXT_A 0
#define PDMA_CHANNEL_NEXT_B 1

/*
 * Bookkeeping structure for each DMA channel.
 */

struct pdma_channel_snapshot {
    int time;

    u8 channel_buf_a_status;
    u32 channel_buf_a_src;
    u32 channel_buf_a_dst;
    u16 channel_buf_a_cnt;

    u8 channel_buf_b_status;
    u32 channel_buf_b_src;
    u32 channel_buf_b_dst;
    u16 channel_buf_b_cnt;

    u32 device_control;
    u32 device_status;

    u32 device_buf_a_src;
    u32 device_buf_a_dst;
    u16 device_buf_a_cnt;

    u32 device_buf_b_src;
    u32 device_buf_b_dst;
    u16 device_buf_b_cnt;
};

#define PDMA_SNAPS_COUNT    64

struct pdma_channel_stats {
    long bytes;
    long xfers_started;
    long xfers_completed;
    struct pdma_channel_snapshot snaps[PDMA_SNAPS_COUNT];
    int snaps_index;
};

struct pdma_channel_t {
    u8 channel;
    u8 in_use;
    u32 flags;
    u8 write_adj;
    u8 next;
    pdma_irq_handler_t handler;
    void* user_data;
    struct pdma_channel_buffer_status_t {
        u32 src;
        u32 dst;
        u16 cnt;
        u8  status;
    } buf[2]; // Buffer A & B
    struct pdma_channel_stats stats;
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



/* NOTE: must be called with lock held! */
void pdma_snap_channel(struct pdma_channel_t *channel)
{
    struct pdma_channel_snapshot* snap;
    u8 ch = channel->channel;
    snap = &channel->stats.snaps[channel->stats.snaps_index];
    snap->time = *STCVR;

    snap->channel_buf_a_status = channel->buf[0].status;
    snap->channel_buf_a_src    = channel->buf[0].src;
    snap->channel_buf_a_dst    = channel->buf[0].dst;
    snap->channel_buf_a_cnt    = channel->buf[0].cnt;

    snap->channel_buf_b_status = channel->buf[1].status;
    snap->channel_buf_b_src    = channel->buf[1].src;
    snap->channel_buf_b_dst    = channel->buf[1].dst;
    snap->channel_buf_b_cnt    = channel->buf[1].cnt;

    snap->device_control = PDMA(pdma_device)->chan[ch].control;
    snap->device_status  = PDMA(pdma_device)->chan[ch].status;

    snap->device_buf_a_src = PDMA(pdma_device)->chan[ch].buf[0].src;
    snap->device_buf_a_dst = PDMA(pdma_device)->chan[ch].buf[0].dst;
    snap->device_buf_a_cnt = PDMA(pdma_device)->chan[ch].buf[0].cnt;

    snap->device_buf_b_src = PDMA(pdma_device)->chan[ch].buf[1].src;
    snap->device_buf_b_dst = PDMA(pdma_device)->chan[ch].buf[1].dst;
    snap->device_buf_b_cnt = PDMA(pdma_device)->chan[ch].buf[1].cnt;

    channel->stats.snaps_index = (channel->stats.snaps_index + 1) & (PDMA_SNAPS_COUNT - 1);

    d_printk(4,  "%08x %d A %s %s src=%08x/%08x dst=%08x/%08x cnt=%08x/%08x\n",
        snap->time,
        ch,
        (snap->channel_buf_a_status == PDMA_CHANNEL_BUFFER_STOPPED)
            ? "stopped" : "started",
        (snap->device_control & PDMA_STATUS_CH_COMP_A) ? "C" : " ",
        snap->device_buf_a_src,
        snap->channel_buf_a_src,
        snap->device_buf_a_dst,
        snap->channel_buf_a_dst,
        snap->device_buf_a_cnt,
        snap->channel_buf_a_cnt);

    d_printk(4, "%08x %d B %s %s src=%08x/%08x dst=%08x/%08x cnt=%08x/%08x\n",
        snap->time,
        ch,
        (channel->buf[1].status == PDMA_CHANNEL_BUFFER_STOPPED)
            ? "stopped" : "started",
        (PDMA(pdma_device)->chan[ch].control & PDMA_STATUS_CH_COMP_B) ? "C" : " ",
        PDMA(pdma_device)->chan[ch].buf[1].src,
        channel->buf[1].dst,
        PDMA(pdma_device)->chan[ch].buf[1].dst,
        channel->buf[1].dst,
        PDMA(pdma_device)->chan[ch].buf[1].cnt,
        channel->buf[1].dst);
}

int pdma_start(u8 ch,
               u32 src,
               u32 dst,
               u16 cnt) {
    unsigned long flags = 0;
    int ret = 0;
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

    // Pause transfer
    PDMA(pdma_device)->chan[ch].control |= PDMA_CONTROL_PAUSE;

    //pdma_snap_channel(channel);

    if ((channel->buf[PDMA_CHANNEL_BUFFER_A].status != PDMA_CHANNEL_BUFFER_STOPPED) && (channel->buf[PDMA_CHANNEL_BUFFER_B].status != PDMA_CHANNEL_BUFFER_STOPPED)) {
        ret = -EBUSY;
        d_printk(0, "busy\n");
        goto pdma_busy;
    }

    // Load source, destination and count
    if (channel->next == PDMA_CHANNEL_NEXT_B && channel->buf[PDMA_CHANNEL_BUFFER_B].status == PDMA_CHANNEL_BUFFER_STOPPED) {
        channel->next = PDMA_CHANNEL_NEXT_A;
        channel->buf[PDMA_CHANNEL_BUFFER_B].status = PDMA_CHANNEL_BUFFER_STARTED;
        channel->buf[PDMA_CHANNEL_BUFFER_B].src = src;
        channel->buf[PDMA_CHANNEL_BUFFER_B].dst = dst;
        channel->buf[PDMA_CHANNEL_BUFFER_B].cnt = cnt;

        PDMA(pdma_device)->chan[ch].buf[PDMA_CHANNEL_BUFFER_B].src = src;
        PDMA(pdma_device)->chan[ch].buf[PDMA_CHANNEL_BUFFER_B].dst = dst;
        PDMA(pdma_device)->chan[ch].buf[PDMA_CHANNEL_BUFFER_B].cnt = cnt;
    }
    else if (channel->next == PDMA_CHANNEL_NEXT_A && channel->buf[PDMA_CHANNEL_BUFFER_A].status == PDMA_CHANNEL_BUFFER_STOPPED) {
        channel->next = PDMA_CHANNEL_NEXT_B;
        channel->buf[PDMA_CHANNEL_BUFFER_A].status = PDMA_CHANNEL_BUFFER_STARTED;

        channel->buf[PDMA_CHANNEL_BUFFER_A].src = src;
        channel->buf[PDMA_CHANNEL_BUFFER_A].dst = dst;
        channel->buf[PDMA_CHANNEL_BUFFER_A].cnt = cnt;

        PDMA(pdma_device)->chan[ch].buf[PDMA_CHANNEL_BUFFER_A].src = src;
        PDMA(pdma_device)->chan[ch].buf[PDMA_CHANNEL_BUFFER_A].dst = dst;
        PDMA(pdma_device)->chan[ch].buf[PDMA_CHANNEL_BUFFER_A].cnt = cnt;
    }
    else {
        ret = -EBUSY;
        d_printk(0, "busy\n");
        goto pdma_busy;
    }
    
    //pdma_snap_channel(channel);

pdma_busy:
    // Resume the transfer
    PDMA(pdma_device)->chan[ch].control &= ~PDMA_CONTROL_PAUSE;
done_pdma_start:
    spin_unlock_irqrestore(&pdma_lock, flags);
    return ret;
} EXPORT_SYMBOL(pdma_start);

// Returns less than zero on error, or how many buffers are available.
int pdma_buffers_available(u8 ch) {
    unsigned long flags = 0;
    struct pdma_channel_t* channel;
    int result = 0;

    if (ch >= 8) {
        d_printk(0, "Invalid channel number %d\n", ch);
        return -EINVAL;
    }

    spin_lock_irqsave(&pdma_lock, flags);

    channel = &pdma_device->channel[ch];
    if (!channel->in_use) {
        spin_unlock_irqrestore(&pdma_lock, flags);
        return result;
    }

    // Pause transfer
    PDMA(pdma_device)->chan[ch].control |= PDMA_CONTROL_PAUSE;

    // See if there's an open channel buffer
    if (channel->buf[PDMA_CHANNEL_BUFFER_A].status == PDMA_CHANNEL_BUFFER_STOPPED)
        result += 1;
    if (channel->buf[PDMA_CHANNEL_BUFFER_B].status == PDMA_CHANNEL_BUFFER_STOPPED)
        result += 1;

    // Resume the transfer
    PDMA(pdma_device)->chan[ch].control &= ~PDMA_CONTROL_PAUSE;

    if (result == 0) {
        d_printk(4, "busy\n");
        pdma_snap_channel(channel);
    }

    spin_unlock_irqrestore(&pdma_lock, flags);

    return result;
} EXPORT_SYMBOL(pdma_buffers_available);

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
    channel->buf[PDMA_CHANNEL_BUFFER_A].status = PDMA_CHANNEL_BUFFER_STOPPED;
    channel->buf[PDMA_CHANNEL_BUFFER_B].status = PDMA_CHANNEL_BUFFER_STOPPED;
    channel->next = PDMA_CHANNEL_NEXT_A;
    memset(&channel->stats, 0, sizeof(struct pdma_channel_stats));
    channel->stats.snaps_index = 0;

    PDMA(pdma_device)->chan[ch].control = PDMA_CONTROL_RESET |
            PDMA_CONTROL_CLR_A |
            PDMA_CONTROL_CLR_B;

    PDMA(pdma_device)->chan[ch].control =
            ((u32)channel->write_adj << 14) | channel->flags;

    //pdma_snap_channel(channel);
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
    unsigned long flags;
    u8 ch;
    u32 status;
    d_printk(2, "irq\n");

    spin_lock_irqsave(&pdma_lock, flags);


    status = PDMA(p)->status;
    for (ch = 0; ch < 8; ++ch) {
        struct pdma_channel_t *channel = &p->channel[ch];
        PDMA(pdma_device)->chan[ch].control |= PDMA_CONTROL_PAUSE;
        if(status & (1 << (ch*2))) {
            d_printk(2, "channel %d buffer A done\n", ch);
            d_printk_pdma_ch(4, ch);
            PDMA(p)->chan[ch].control |= PDMA_CONTROL_CLR_A;
            channel->buf[PDMA_CHANNEL_BUFFER_A].status = PDMA_CHANNEL_BUFFER_STOPPED;
        }
        if(status & (2 << (ch*2))) {
            d_printk(2, "channel %d buffer B done\n", ch);
            d_printk_pdma_ch(4, ch);
            PDMA(p)->chan[ch].control |= PDMA_CONTROL_CLR_B;
            channel->buf[PDMA_CHANNEL_BUFFER_B].status = PDMA_CHANNEL_BUFFER_STOPPED;
        }
        if ((status & (3 << (ch*2)) && p->channel[ch].handler)) {
            p->channel[ch].handler(p->channel[ch].user_data);
        }
        PDMA(pdma_device)->chan[ch].control &= ~PDMA_CONTROL_PAUSE;
    }


    spin_unlock_irqrestore(&pdma_lock, flags);

	return IRQ_HANDLED;
}

static int pdma_channel_show(struct seq_file *m, void *private)
{
    unsigned long flags;
    int ch = 0;
    struct pdma_channel_t *channel;
    int i;

    // Lock & pause the channel
    spin_lock_irqsave(&pdma_lock, flags);
    channel = &pdma_device->channel[ch];
    PDMA(pdma_device)->chan[ch].control |= PDMA_CONTROL_PAUSE;

    for (i = 0; i < PDMA_SNAPS_COUNT; ++i) {
        int offset = (channel->stats.snaps_index - i) & (PDMA_SNAPS_COUNT - 1);
        struct pdma_channel_stats *stats = &channel->stats;
        struct pdma_channel_snapshot *snap = &(stats->snaps[offset]);
        if (snap->time == 0)
            continue;
        seq_printf(m, "%08x A %s %s src=%08x/%08x dst=%08x/%08x cnt=%08x/%08x\n",
            snap->time,
            (snap->channel_buf_a_status == PDMA_CHANNEL_BUFFER_STOPPED)
                ? "stopped" : "started",
            (snap->device_control & PDMA_STATUS_CH_COMP_A) ? "C" : " ",
            snap->device_buf_a_src,
            snap->channel_buf_a_src,
            snap->device_buf_a_dst,
            snap->channel_buf_a_dst,
            snap->device_buf_a_cnt,
            snap->channel_buf_a_cnt);

        seq_printf(m, "%08x B %s %s src=%08x/%08x dst=%08x/%08x cnt=%08x/%08x\n",
            snap->time,
            (channel->buf[1].status == PDMA_CHANNEL_BUFFER_STOPPED)
                ? "stopped" : "started",
            (PDMA(pdma_device)->chan[ch].control & PDMA_STATUS_CH_COMP_B) ? "C" : " ",
            PDMA(pdma_device)->chan[ch].buf[1].src,
            channel->buf[1].dst,
            PDMA(pdma_device)->chan[ch].buf[1].dst,
            channel->buf[1].dst,
            PDMA(pdma_device)->chan[ch].buf[1].cnt,
            channel->buf[1].dst);
    }

    // Unlock & resume the channel
    PDMA(pdma_device)->chan[ch].control &= ~PDMA_CONTROL_PAUSE;
    spin_unlock_irqrestore(&pdma_lock, flags);

    return 0;
}

static int pdma_channel_open(struct inode *inode, struct file *file)
{
    return single_open(file, pdma_channel_show, inode->i_private);
}


static const struct file_operations pdma_channel_fops = {
    .owner = THIS_MODULE,
    .open = pdma_channel_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

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
		goto free_pdma_device;
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


    pdma_debugfs_root = debugfs_create_dir("pdma", NULL);
    if (!pdma_debugfs_root) {
        d_printk(0, "cannot create debugfs dir\n");
        ret = -ENXIO;
        goto fail_debugfs_create_dir;
    }
    
    debugfs_create_file("channel0", S_IRUGO, pdma_debugfs_root,
                        (void*)0, &pdma_channel_fops);

    goto done;

fail_debugfs_create_dir:
error_release_irq:
    free_irq(pdma_device->irq, pdma_device);
error_release_pdma_regs:
	iounmap(pdma_device->regs);
free_pdma_device:
    kfree(pdma_device);
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
