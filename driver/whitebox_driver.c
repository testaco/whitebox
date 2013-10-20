#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <mach/fpga.h>

#include "pdma.h"
#include "whitebox.h"
#include "whitebox_gpio.h"
#include "whitebox_ring_buffer.h"

#define WHITEBOX_EXCITER_IRQ A2F_FPGA_DEMUX_IRQ_MAP(0)

/*
 * Book-keeping for the device
 */
struct whitebox_device_t {
    struct semaphore sem;
    struct cdev cdev;
    struct device* device;
    void* exciter;
    int irq;
    int irq_disabled;
    struct whitebox_ring_buffer tx_rb;
    struct whitebox_platform_data_t* platform_data;
    u8 tx_dma_ch;
    u8 tx_dma_active;
    wait_queue_head_t write_wait_queue;
    u32 adf4351_regs[WA_REGS_COUNT];
} *whitebox_device;

/*
 * IO Mapped structure of the exciter
 */
struct whitebox_exciter_regs_t {
    u32 sample;
    u32 state;
    u32 interp;
    u32 fcw;
    u32 runs;
    u32 threshold;
};

#define WHITEBOX_EXCITER(s) ((volatile struct whitebox_exciter_regs_t *)(s->exciter))

static dev_t whitebox_devno;
static struct class* whitebox_class;

/*
 * Ensures that only one program can open the device at a time
 */
static atomic_t use_count = ATOMIC_INIT(0);

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int whitebox_debug = 0;

/*
 * User can change verbosity of the driver
 */
module_param(whitebox_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(whitebox_debug, "whitebox debugging level, >0 is verbose");


/*
 * Number of pages for the ring buffer
 */
static unsigned whitebox_num_pages = 8;

/*
 * User can change the number of pages for the ring buffer
 */
//module_param(whitebox_num_pages, unsigned, S_IRUSR | S_IWUSR);
//MODULE_PARAM_DESC(whitebox_num_pages, "number of pages for the ring buffer");

static u8 whitebox_cmx991_regs_write_lut[] = {
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x20, 0x21, 0x22, 0x23
};
static u8 whitebox_cmx991_regs_read_lut[] = {
    0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xd0, 0xd1, 0xd2, 0xd3
};

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (whitebox_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void d_printk_wb(int level, struct whitebox_device_t* wb) {
    u32 state;
    state = WHITEBOX_EXCITER(wb)->state;
    state = WHITEBOX_EXCITER(wb)->state;

    d_printk(level, "%c%c%c%c readable=%d active=%d\n",
        state & WES_TXEN ? 'T' : ' ',
        state & WES_DDSEN ? 'D' : ' ',
        state & WES_AFULL ? 'F' : ' ',
        state & WES_AEMPTY ? 'E' : ' ',
        wb->tx_rb.readable_pages,
        wb->tx_rb.dma_active);
}

/* Prototpyes */
long whitebox_ioctl_reset(void);

int tx_start(struct whitebox_device_t* wb) {
    int count = 0;
    dma_addr_t mapping;
    u32 state;

    state = WHITEBOX_EXCITER(wb)->state;
    state = WHITEBOX_EXCITER(wb)->state;
    if (state & WES_AFULL) {
        if (!(state & WES_TXEN)) {
            WHITEBOX_EXCITER(wb)->state = state | WES_TXEN;
            d_printk(1, "afull, txen\n");
        }
        return count;
    }

    count = whitebox_ring_buffer_read_dma_start(&wb->tx_rb,
            &mapping);

    if (count < 0) {
        d_printk(0, "Couldn't dma map ring buffer for xfer\n");
    }
    else if (count > 0) {
        d_printk(3, "%d\n", count >> 2);

        d_printk_wb(2, wb);
        d_printk(1, "starting tx\n");

        pdma_start(wb->tx_dma_ch,
                mapping,
                (u32)&WHITEBOX_EXCITER(wb)->sample,
                count >> 2);
        if (wb->irq_disabled) {
            //enable_irq(wb->irq);
            wb->irq_disabled = 0;
        }
    } else {
        d_printk(1, "nothing to tx\n");
        if (!wb->irq_disabled) {
            d_printk(1, "nothing to tx, disabling irq\n");
            //disable_irq_nosync(wb->irq);
            d_printk(1, "hey\n");
            wb->irq_disabled = 1;
            d_printk(1, "wake_up\n");
            //wake_up_interruptible(&wb->write_wait_queue);
        }
    }

    d_printk_wb(4, wb);
    return count;
}

void tx_dma_cb(void* data) {
    struct whitebox_device_t* wb = (struct whitebox_device_t*)data;
    //u32 state;
    d_printk(1, "tx dma cb\n");

    whitebox_ring_buffer_read_dma_finish(&wb->tx_rb);

    d_printk(1, "wake_up\n");

    wake_up_interruptible(&wb->write_wait_queue);

    tx_start(wb);

    d_printk_wb(4, wb);
}

static irqreturn_t tx_irq_cb(int irq, void* ptr) {
    struct whitebox_device_t* wb = (struct whitebox_device_t*)ptr;
    d_printk(1, "clearing txirq\n");
    WHITEBOX_EXCITER(wb)->state = WES_CLEAR_TXIRQ;

    //d_printk_wb(0, wb);

    tx_start(wb);

    return IRQ_HANDLED;
}

static int whitebox_open(struct inode* inode, struct file* filp) {
    int ret = 0;
    d_printk(2, "whitebox open\n");

    if (atomic_add_return(1, &use_count) != 1) {
        d_printk(0, "Device in use\n");
        ret = -EBUSY;
        goto fail_in_use;
    }

    whitebox_ioctl_reset();

    if (filp->f_flags & O_WRONLY || filp->f_flags & O_RDWR) {
        // init dma channel
        ret = pdma_request(whitebox_device->tx_dma_ch,
                (pdma_irq_handler_t)tx_dma_cb,
                whitebox_device,
                10,
                PDMA_CONTROL_PER_SEL_FPGA0 |
                PDMA_CONTROL_HIGH_PRIORITY |
                PDMA_CONTROL_XFER_SIZE_4B |
                PDMA_CONTROL_DST_ADDR_INC_0 |
                PDMA_CONTROL_SRC_ADDR_INC_4 |
                PDMA_CONTROL_PERIPH |
                PDMA_CONTROL_DIR_MEM_TO_PERIPH |
                PDMA_CONTROL_INTEN);
        if (ret < 0) {
            d_printk(0, "DMA Channel request failed\n");
            goto fail_in_use;
        }

        // init ring buffers
        whitebox_ring_buffer_init(&whitebox_device->tx_rb);

        whitebox_device->tx_dma_active = 0;

        // enable dac
        whitebox_gpio_dac_enable(whitebox_device->platform_data);
    }

    goto done_open;

fail_in_use:
    atomic_dec(&use_count);
done_open:
    return ret;
}

static int whitebox_release(struct inode* inode, struct file* filp) {
    u32 state;
    d_printk(2, "whitebox release\n");
    
    if (atomic_read(&use_count) != 1) {
        d_printk(0, "Device not in use");
        return -ENOENT;
    }

    if (filp->f_flags & O_WRONLY || filp->f_flags & O_RDWR) {
        // wait for DMA to finish
        /*while (pdma_active(whitebox_device->tx_dma_ch) > 0) {
            cpu_relax();
        }*/

        whitebox_ioctl_reset();

        // Disable DAC
        whitebox_gpio_dac_disable(whitebox_device->platform_data);

        // release dma channel
        pdma_release(whitebox_device->tx_dma_ch);

        // Turn off transmission
        state = WHITEBOX_EXCITER(whitebox_device)->state;
        state = WHITEBOX_EXCITER(whitebox_device)->state;
        WHITEBOX_EXCITER(whitebox_device)->state = state & ~WES_TXEN;
    }

    atomic_dec(&use_count);

    return 0;
}

static int whitebox_read(struct file* filp, char __user* buf, size_t count, loff_t* pos) {
    d_printk(2, "whitebox read\n");
    return 0;
}

static int whitebox_write(struct file* filp, const char __user* buf, size_t count, loff_t* pos) {
    int ret = 0;
    
    d_printk(2, "whitebox write\n");

    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }
    

    while (whitebox_ring_buffer_writeable_pages(&whitebox_device->tx_rb) <= 0) {
        up(&whitebox_device->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;

        tx_start(whitebox_device);

        if (wait_event_interruptible(whitebox_device->write_wait_queue,
                whitebox_ring_buffer_writeable_pages(
                    &whitebox_device->tx_rb) > 0))
            return -ERESTARTSYS;
        if (down_interruptible(&whitebox_device->sem)) {
            return -ERESTARTSYS;
        }
    }

    d_printk(1, "hi");

    ret = whitebox_ring_buffer_write_from_user(&whitebox_device->tx_rb,
            buf, count);

    if (ret < 0) {
        up(&whitebox_device->sem);
        return -EFAULT;
    }

    up(&whitebox_device->sem);

    tx_start(whitebox_device);

    return count;
}

long whitebox_ioctl_reset(void) {
    int i;
    whitebox_gpio_cmx991_reset(whitebox_device->platform_data);
    for (i = 0; i < WA_REGS_COUNT; ++i) {
        whitebox_device->adf4351_regs[i] = 0;
    }
    WHITEBOX_EXCITER(whitebox_device)->state = WES_CLEAR;
    return 0;
}

long whitebox_ioctl_locked(unsigned long arg) {
    whitebox_args_t w;
    u8 c;
    c = whitebox_gpio_cmx991_read(whitebox_device->platform_data,
        WHITEBOX_CMX991_LD_REG);
    w.locked = whitebox_gpio_adf4351_locked(whitebox_device->platform_data)
            && (c & WHITEBOX_CMX991_LD_MASK);
    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_exciter_clear(void) {
    WHITEBOX_EXCITER(whitebox_device)->state = WES_CLEAR;
    return 0;
}

long whitebox_ioctl_exciter_get(unsigned long arg) {
    whitebox_args_t w;
    w.flags.exciter.state = WHITEBOX_EXCITER(whitebox_device)->state;
    w.flags.exciter.state = WHITEBOX_EXCITER(whitebox_device)->state;
    w.flags.exciter.interp = WHITEBOX_EXCITER(whitebox_device)->interp;
    w.flags.exciter.interp = WHITEBOX_EXCITER(whitebox_device)->interp;
    w.flags.exciter.fcw = WHITEBOX_EXCITER(whitebox_device)->fcw;
    w.flags.exciter.fcw = WHITEBOX_EXCITER(whitebox_device)->fcw;
    w.flags.exciter.runs = WHITEBOX_EXCITER(whitebox_device)->runs;
    w.flags.exciter.runs = WHITEBOX_EXCITER(whitebox_device)->runs;
    w.flags.exciter.threshold = WHITEBOX_EXCITER(whitebox_device)->threshold;
    w.flags.exciter.threshold = WHITEBOX_EXCITER(whitebox_device)->threshold;
    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_exciter_set(unsigned long arg) {
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;
    WHITEBOX_EXCITER(whitebox_device)->state = w.flags.exciter.state;
    WHITEBOX_EXCITER(whitebox_device)->interp = w.flags.exciter.interp;
    WHITEBOX_EXCITER(whitebox_device)->fcw = w.flags.exciter.fcw;
    WHITEBOX_EXCITER(whitebox_device)->threshold = w.flags.exciter.threshold;
    return 0;
}

long whitebox_ioctl_cmx991_get(unsigned long arg) {
    whitebox_args_t w;
    int i;
    for (i = 0; i < WC_REGS_COUNT; ++i)
        w.flags.cmx991[i] = whitebox_gpio_cmx991_read(
                whitebox_device->platform_data, 
                whitebox_cmx991_regs_read_lut[i]);

    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_cmx991_set(unsigned long arg) {
    whitebox_args_t w;
    int i;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;

    for (i = 0; i < WC_REGS_COUNT; ++i)
        whitebox_gpio_cmx991_write(whitebox_device->platform_data, 
                whitebox_cmx991_regs_write_lut[i],
                w.flags.cmx991[i]);

    return 0;
}

long whitebox_ioctl_adf4351_get(unsigned long arg) {
    whitebox_args_t w;
    int i;
    for (i = 0; i < WA_REGS_COUNT; ++i)
        w.flags.adf4351[i] = whitebox_device->adf4351_regs[i];

    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_adf4351_set(unsigned long arg) {
    whitebox_args_t w;
    int i;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;

    for (i = WA_REGS_COUNT - 1; i >= 0; --i) {
        whitebox_device->adf4351_regs[i] = w.flags.adf4351[i];
        whitebox_gpio_adf4351_write(whitebox_device->platform_data, 
                w.flags.adf4351[i]);
        d_printk(0, "\n[adf4351] %d %x\n", i, w.flags.adf4351[i]);
    }

    return 0;
}

static long whitebox_ioctl(struct file* filp, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case W_RESET:
            return whitebox_ioctl_reset();
        case W_LOCKED:
            return whitebox_ioctl_locked(arg);
        case WE_CLEAR:
            return whitebox_ioctl_exciter_clear();
        case WE_GET:
            return whitebox_ioctl_exciter_get(arg);
        case WE_SET:
            return whitebox_ioctl_exciter_set(arg);
        case WC_GET:
            return whitebox_ioctl_cmx991_get(arg);
        case WC_SET:
            return whitebox_ioctl_cmx991_set(arg);
        case WA_GET:
            return whitebox_ioctl_adf4351_get(arg);
        case WA_SET:
            return whitebox_ioctl_adf4351_set(arg);
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations whitebox_fops = {
    .owner = THIS_MODULE,
    .open = whitebox_open,
    .release = whitebox_release,
    .read = whitebox_read,
    .write = whitebox_write,
    .unlocked_ioctl = whitebox_ioctl,
};

static int whitebox_probe(struct platform_device* pdev) {
    struct resource* whitebox_exciter_regs;
    int irq;
    struct device* dev;
    int ret = 0;

    d_printk(2, "whitebox probe\n");

    whitebox_exciter_regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!whitebox_exciter_regs) {
        d_printk(0, "no register base for Whitebox exciter\n");
        ret = -ENXIO;
        goto fail_release_nothing;
    }

    irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        d_printk(0, "invalid IRQ%d\n", irq);
        ret = -ENXIO;
        goto fail_release_nothing;
    }

    whitebox_device = kzalloc(sizeof(struct whitebox_device_t), GFP_KERNEL);

    sema_init(&whitebox_device->sem, 1);

    whitebox_device->exciter = ioremap(whitebox_exciter_regs->start,
            resource_size(whitebox_exciter_regs));
    if (!whitebox_device->exciter) {
		d_printk(0, "unable to map registers for "
			"whitebox exciter base=%08x\n", whitebox_exciter_regs->start);
		ret = -EINVAL;
		goto fail_ioremap;
    }

    d_printk(2, "Mapped exciter to address %08x\n", whitebox_exciter_regs->start);

    whitebox_device->irq = irq;
    /*ret = request_irq(irq, tx_irq_cb, 0,
            dev_name(&pdev->dev), whitebox_device);
    if (ret) {
        d_printk(0, "request irq %d failed for whitebox\n", irq);
        ret = -EINVAL;
        goto fail_irq;
    }*/
    whitebox_device->irq_disabled = 0;

    whitebox_class = class_create(THIS_MODULE, WHITEBOX_DRIVER_NAME);
    if (IS_ERR(whitebox_class)) {
        d_printk(0, "Failed to create the whitebox device class\n");
        ret = -EINVAL;
        goto fail_create_class;
    }

    ret = alloc_chrdev_region(&whitebox_devno, 0, 1, WHITEBOX_DRIVER_NAME);
    if (ret < 0) {
        d_printk(0, "Failed to allocate device region\n");
        goto fail_alloc_region;
    }

    d_printk(1, "cdev major=%d minor=%d\n", MAJOR(whitebox_devno),
        MINOR(whitebox_devno));

    cdev_init(&whitebox_device->cdev, &whitebox_fops);
    whitebox_device->cdev.owner = THIS_MODULE;
    ret = cdev_add(&whitebox_device->cdev, whitebox_devno, 1);
    if (ret < 0) {
        d_printk(0, "Failed to create the whitebox character device\n");
        goto fail_create_cdev;
    }

    dev = device_create(whitebox_class, NULL, whitebox_devno,
            "%s", WHITEBOX_DRIVER_NAME);
    if (IS_ERR(dev)) {
        d_printk(0, "Failed to create the whitebox device\n");
        ret = -EINVAL;
        goto fail_create_device;
    }
    whitebox_device->device = dev;

    ret = whitebox_gpio_request(pdev);
    if (ret < 0) {
        d_printk(0, "Failed to allocate GPIOs\n");
        goto fail_gpio_request;
    }

    ret = whitebox_ring_buffer_alloc(&whitebox_device->tx_rb, whitebox_num_pages);
    if (ret < 0) {
        d_printk(0, "Error allocating the transmit ring buffer\n");
        ret = -ENOMEM;
        goto fail_alloc_ring_buffers;
    }

    whitebox_device->tx_dma_ch = WHITEBOX_PLATFORM_DATA(pdev)->tx_dma_ch;
    whitebox_device->platform_data = WHITEBOX_PLATFORM_DATA(pdev);

    whitebox_gpio_cmx991_reset(whitebox_device->platform_data);

    init_waitqueue_head(&whitebox_device->write_wait_queue);

    goto done;

fail_alloc_ring_buffers:
    whitebox_gpio_free(pdev);
fail_gpio_request:
    device_destroy(whitebox_class, whitebox_devno);
fail_create_device:
fail_create_cdev:
    unregister_chrdev_region(whitebox_devno, 1);

fail_alloc_region:
    class_destroy(whitebox_class);

fail_create_class:
/*    free_irq(whitebox_device->irq, whitebox_device);
fail_irq:*/
    iounmap(whitebox_device->exciter);
fail_ioremap:
    kfree(whitebox_device);

fail_release_nothing:
done:
    return ret;
}

static int whitebox_remove(struct platform_device* pdev) {
    d_printk(2, "whitebox remove\n");

    whitebox_ring_buffer_free(&whitebox_device->tx_rb);

    whitebox_gpio_free(pdev);

    cdev_del(&whitebox_device->cdev);

    device_destroy(whitebox_class, whitebox_devno);

    unregister_chrdev_region(whitebox_devno, 1);

    class_destroy(whitebox_class);

    //free_irq(whitebox_device->irq, whitebox_device);

    iounmap(whitebox_device->exciter);

    kfree(whitebox_device);
    return 0;
}

static int whitebox_suspend(struct platform_device* pdev, pm_message_t state) {
    d_printk(2, "whitebox suspend\n");
    return 0;
}

static int whitebox_resume(struct platform_device* pdev) {
    d_printk(2, "whitebox resume\n");
    return 0;
}

static struct platform_driver whitebox_platform_driver = {
    .probe = whitebox_probe,
    .remove = whitebox_remove,
    .suspend = whitebox_suspend,
    .resume = whitebox_resume,
    .driver = {
        .name = WHITEBOX_DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

static struct platform_device* whitebox_platform_device;

/*
 * These whitebox ioresource mappings are derived from the Whitebox
 * Libero SmartDesign.
 */
static struct resource whitebox_platform_device_resources[] = {
    {
        .start = WHITEBOX_EXCITER_REGS,
        .end = WHITEBOX_EXCITER_REGS + WHITEBOX_EXCITER_REGS_COUNT,
        .flags = IORESOURCE_MEM,
    }, {
        .start = WHITEBOX_EXCITER_IRQ,
        .flags = IORESOURCE_IRQ,
    },
};

/*
 * These whitebox pin to Linux kernel GPIO mappings are derived from the
 * Whitebox Libero SmartDesign.
 */
static struct whitebox_platform_data_t whitebox_platform_data = {
    .adc_s1_pin         = 36,
    .adc_s2_pin         = 35,
    .adc_dfs_pin        = 37,
    .dac_en_pin         = 38,
    .dac_pd_pin         = 39,
    .dac_cs_pin         = 40,
    .radio_resetn_pin   = 41,
    .radio_cdata_pin    = 42,
    .radio_sclk_pin     = 43,
    .radio_rdata_pin    = 44,
    .radio_csn_pin      = 45,
    .vco_clk_pin        = 46,
    .vco_data_pin       = 47,
    .vco_le_pin         = 48,
    .vco_ce_pin         = 49,
    .vco_pdb_pin        = 50,
    .vco_ld_pin         = 51,
    .tx_dma_ch          = 0,
};

static int __init whitebox_init_module(void) {
    int ret;
    d_printk(2, "whitebox init module\n");
    ret = platform_driver_register(&whitebox_platform_driver);
    if (ret < 0) {
        d_printk(0, "Couldn't register driver");
        goto failed_register_driver;
    }

    whitebox_platform_device = platform_device_alloc("whitebox", 0);
    if (!whitebox_platform_device) {
        d_printk(0, "Couldn't allocate device");
        ret = -ENOMEM;
        goto failed_create_platform_device;
    }

    whitebox_platform_device->num_resources =
            ARRAY_SIZE(whitebox_platform_device_resources);
    whitebox_platform_device->resource = whitebox_platform_device_resources;
    whitebox_platform_device->dev.platform_data = &whitebox_platform_data;

    ret = platform_device_add(whitebox_platform_device);
    if (ret < 0) {
        d_printk(0, "Couldn't add device");
        goto failed_create_platform_device;
    }

    goto done_init_module;

failed_create_platform_device:
    platform_driver_unregister(&whitebox_platform_driver);

failed_register_driver:
done_init_module:
    return ret;
}

static void __exit whitebox_cleanup_module(void) {
    d_printk(2, "whitebox cleanup\n");
    platform_device_unregister(whitebox_platform_device);
    platform_driver_unregister(&whitebox_platform_driver);
}

module_init(whitebox_init_module);
module_exit(whitebox_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Testa, chris@testa.co");
MODULE_DESCRIPTION("Whitebox software defined radio");
