#include <asm/uaccess.h>
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
#include "whitebox_block.h"

static struct whitebox_device *whitebox_device;
static dev_t whitebox_devno;
static struct class* whitebox_class;

/*
 * Ensures that only one program can open the device at a time
 */
static atomic_t use_count = ATOMIC_INIT(0);

/*
 * Driver verbosity level: 0->silent; >0->verbose
 * User can change verbosity of the driver.
 */
static int whitebox_debug = 0;
module_param(whitebox_debug, int, S_IRUSR | S_IWUSR);

/*
 * Does a mis-locked PLL cause a file error?
 */
static int whitebox_check_plls = 1;
module_param(whitebox_check_plls, int, S_IRUSR | S_IWUSR);

/*
 * Does an overrun or underrun cause a file error?
 */
static int whitebox_check_runs = 1;
module_param(whitebox_check_runs, int, S_IRUSR | S_IWUSR);

/*
 * Order of the user_source circular buffer.
 */
static int whitebox_user_source_order = 3;
module_param(whitebox_user_source_order, int, S_IRUSR | S_IWUSR);

/*
 * Whether or not to use the mock exciter.
 */
static int whitebox_mock_exciter_en = 0;
module_param(whitebox_mock_exciter_en, int, S_IRUSR | S_IWUSR);

/*
 * Order of the mock exciter circular buffer
 */
static int whitebox_mock_exciter_order = 3;
module_param(whitebox_mock_exciter_order, int, S_IRUSR | S_IWUSR);

/*
 * Block size of dma xfers to the exciter peripheral.
 */
static int whitebox_exciter_quantum = 64;
module_param(whitebox_exciter_quantum, int, S_IRUSR | S_IWUSR);

/*
 * Register mappings for the CMX991 register file.
 */
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

/* Prototpyes */
long whitebox_ioctl_reset(void);

int tx_start(struct whitebox_device *wb)
{
    struct whitebox_exciter *exciter = wb->rf_sink.exciter;
    exciter->ops->set_state(exciter, WES_CLEAR);
    exciter->ops->get_runs(exciter, &wb->cur_overruns, &wb->cur_underruns);
    return 0;
}

int tx_exec(struct whitebox_device* wb) {
    struct whitebox_user_source *user_source = &wb->user_source;
    struct whitebox_rf_sink *rf_sink = &wb->rf_sink;
    size_t src_count, dest_count;
    unsigned long src, dest;
    size_t count;
    int result;

    if (!spin_trylock(&rf_sink->lock))
        return -EBUSY;

    src_count = whitebox_user_source_data_available(user_source, &src);
    dest_count = whitebox_rf_sink_space_available(rf_sink, &dest);
    count = min(src_count, dest_count);
    
    if (count <= 0) {
        spin_unlock(&rf_sink->lock);
        return -EBUSY;
    }

    result = whitebox_rf_sink_work(rf_sink, src, src_count, dest, dest_count);

    d_printk(2, "src=%08lx src_count=%zu dest=%08lx dest_count=%zu result=%d\n",
            src, src_count, dest, dest_count, result);

    if (result < 0) {
        spin_unlock(&rf_sink->lock);
        d_printk(0, "rf_sink_work error!\n");
        return result;
    }

    return result;
}

void tx_dma_cb(void *data) {
    struct whitebox_device *wb = (struct whitebox_device *)data;
    struct whitebox_user_source *user_source = &wb->user_source;
    struct whitebox_rf_sink *rf_sink = &wb->rf_sink;
    size_t count;
    d_printk(1, "tx dma cb\n");

    count = whitebox_rf_sink_work_done(rf_sink);
    whitebox_user_source_consume(user_source, count);
    whitebox_rf_sink_produce(rf_sink, count);

    spin_unlock(&rf_sink->lock);

    d_printk(1, "wake_up\n");

    wake_up_interruptible(&wb->write_wait_queue);

    tx_exec(wb);
}

void tx_stop(struct whitebox_device *wb) {
    d_printk(2, "\n");
    wb->rf_sink.exciter->ops->set_state(wb->rf_sink.exciter, WES_TXSTOP);
}

int tx_error(struct whitebox_device *wb) {
    if (whitebox_check_plls) {
        int c, locked;
        c = whitebox_gpio_cmx991_read(wb->platform_data,
            WHITEBOX_CMX991_LD_REG);
        locked = whitebox_gpio_adf4351_locked(wb->platform_data)
                && (c & WHITEBOX_CMX991_LD_MASK);
        if (!locked)
            return 1;
    }

    if (whitebox_check_runs) {
        u16 overruns, underruns;
        wb->rf_sink.exciter->ops->get_runs(wb->rf_sink.exciter,
                &overruns, &underruns);
        if (wb->cur_overruns != overruns) {
            wb->cur_overruns = overruns;
            return 2;
        }
        if (wb->cur_underruns != underruns) {
            wb->cur_underruns = underruns;
            return 3;
        }
    }
    return 0;
}

static irqreturn_t tx_irq_cb(int irq, void* ptr) {
    struct whitebox_device* wb = (struct whitebox_device*)ptr;
    d_printk(1, "clearing txirq\n");

    tx_exec(wb);

    return IRQ_HANDLED;
}

static int whitebox_open(struct inode* inode, struct file* filp) {
    int ret = 0;
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    struct whitebox_rf_sink *rf_sink = &whitebox_device->rf_sink;
    struct whitebox_exciter *exciter = whitebox_mock_exciter_en ?
            &whitebox_device->mock_exciter.exciter :
            &whitebox_device->exciter;

    d_printk(2, "whitebox open\n");

    if (atomic_add_return(1, &use_count) != 1) {
        d_printk(1, "Device in use\n");
        ret = -EBUSY;
        goto fail_in_use;
    }

    whitebox_user_source_init(&whitebox_device->user_source,
            whitebox_user_source_order, &whitebox_device->mapped);

    whitebox_rf_sink_init(&whitebox_device->rf_sink,
            whitebox_device->platform_data->tx_dma_ch,
            tx_dma_cb,
            whitebox_device,
            exciter);

    exciter->quantum = whitebox_exciter_quantum;

    if (filp->f_flags & O_WRONLY || filp->f_flags & O_RDWR) {
        ret = whitebox_rf_sink_alloc(rf_sink);
        if (ret < 0) {
            d_printk(0, "DMA Channel request failed\n");
            goto fail_in_use;
        }

        ret = whitebox_user_source_alloc(user_source);
        if (ret < 0) {
            d_printk(0, "Buffer allocation failed\n");
            goto fail_free_rf_sink;
        }

        // enable dac
        //whitebox_gpio_dac_enable(whitebox_device->platform_data);
    }

    whitebox_device->state = WDS_IDLE;

    whitebox_ioctl_reset();

    goto done_open;

fail_free_rf_sink:
    whitebox_rf_sink_free(rf_sink);
fail_in_use:
    atomic_dec(&use_count);
done_open:
    return ret;
}

static int whitebox_release(struct inode* inode, struct file* filp) {
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    struct whitebox_rf_sink *rf_sink = &whitebox_device->rf_sink;
    d_printk(2, "whitebox release\n");
    
    if (atomic_read(&use_count) != 1) {
        d_printk(0, "Device not in use");
        return -ENOENT;
    }

    if (whitebox_device->state == WDS_TX)
        tx_stop(whitebox_device);

    if (filp->f_flags & O_WRONLY || filp->f_flags & O_RDWR) {
        // wait for DMA to finish
        /*while (pdma_active(whitebox_device->tx_dma_ch) > 0) {
            cpu_relax();
        }*/

        //whitebox_ioctl_reset();

        // Disable DAC
        //whitebox_gpio_dac_disable(whitebox_device->platform_data);

        whitebox_rf_sink_free(rf_sink);
        whitebox_user_source_free(user_source);

        // Turn off transmission
        //rf_sink->exciter->ops->clear_state(rf_sink->exciter, WES_TXEN);
    }

    atomic_dec(&use_count);
    return 0;
}

static int whitebox_read(struct file* filp, char __user* buf, size_t count, loff_t* pos) {
    d_printk(2, "whitebox read\n");
    return 0;
}

static int whitebox_write(struct file* filp, const char __user* buf, size_t count, loff_t* pos) {
    unsigned long dest;
    size_t dest_count;
    int ret = 0;
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    
    d_printk(2, "whitebox write\n");

    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }

    if (whitebox_device->state == WDS_RX) {
        up(&whitebox_device->sem);
        d_printk(1, "in receive\n");
        return -EBUSY;
    }

    if (whitebox_device->state == WDS_IDLE) {
        whitebox_device->state = WDS_TX;
        tx_start(whitebox_device);
    }

    if (count == 0) {
        tx_stop(whitebox_device);
        up(&whitebox_device->sem);
        return 0;
    }

    if (tx_error(whitebox_device)) {
        d_printk(1, "tx_error\n");
        up(&whitebox_device->sem);
        return -EFAULT;
    }

    while ((dest_count = whitebox_user_source_space_available(user_source, &dest)) == 0) {
        up(&whitebox_device->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (wait_event_interruptible(whitebox_device->write_wait_queue,
                (dest_count = whitebox_user_source_space_available(user_source, &dest)) > 0))
            return -ERESTARTSYS;
        if (down_interruptible(&whitebox_device->sem)) {
            return -ERESTARTSYS;
        }
    }

    d_printk(1, "hi");

    ret = whitebox_user_source_work(user_source, (unsigned long)buf, count, dest, dest_count);

    if (ret < 0) {
        up(&whitebox_device->sem);
        return -EFAULT;
    }

    whitebox_user_source_produce(user_source, ret);

    up(&whitebox_device->sem);

    tx_exec(whitebox_device);

    return ret;
}

static int whitebox_fsync(struct file *file, struct dentry *dentry, int datasync)
{
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }
    if (whitebox_device->state == WDS_TX) {
        if (tx_error(whitebox_device)) {
            up(&whitebox_device->sem);
            return -EFAULT;
        }
        tx_stop(whitebox_device);
        while (exciter->ops->get_state(exciter) & WES_TXEN)
            cpu_relax();
        whitebox_device->state = WDS_IDLE;
    }
    up(&whitebox_device->sem);
    return 0;
}

long whitebox_ioctl_reset(void) {
    int i;
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_gpio_cmx991_reset(whitebox_device->platform_data);
    for (i = 0; i < WA_REGS_COUNT; ++i) {
        whitebox_device->adf4351_regs[i] = 0;
    }
    exciter->ops->set_state(exciter, WES_CLEAR);
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
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    exciter->ops->set_state(exciter, WES_CLEAR);
    return 0;
}

long whitebox_ioctl_exciter_get(unsigned long arg) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_args_t w;
    w.flags.exciter.state = exciter->ops->get_state(exciter);
    w.flags.exciter.interp = exciter->ops->get_interp(exciter);
    w.flags.exciter.fcw = exciter->ops->get_fcw(exciter);
    // TODO w.flags.exciter.runs = exciter->ops->get_runs(exciter);
    w.flags.exciter.threshold = exciter->ops->get_threshold(exciter);
    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_exciter_set(unsigned long arg) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;
    exciter->ops->set_state(exciter, w.flags.exciter.state);
    exciter->ops->set_interp(exciter, w.flags.exciter.interp);
    exciter->ops->set_fcw(exciter, w.flags.exciter.fcw);
    exciter->ops->set_threshold(exciter, w.flags.exciter.threshold);
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

long whitebox_ioctl_mock_exciter_command(unsigned long arg) {
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;

    if (w.mock_command == WMC_CAUSE_UNDERRUN) {
        WHITEBOX_EXCITER(&whitebox_device->mock_exciter.exciter)->runs +=
                0x00000001;
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
        case WM_CMD:
            return whitebox_ioctl_mock_exciter_command(arg);
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
    .fsync = whitebox_fsync,
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

    whitebox_device = kzalloc(sizeof(struct whitebox_device), GFP_KERNEL);

    sema_init(&whitebox_device->sem, 1);

    init_waitqueue_head(&whitebox_device->write_wait_queue);

    if ((ret = whitebox_exciter_create(&whitebox_device->exciter,
            whitebox_exciter_regs->start, 
            resource_size(whitebox_exciter_regs)))) {
        goto fail_create_exciter;
    }

    if ((ret = whitebox_mock_exciter_create(&whitebox_device->mock_exciter,
            resource_size(whitebox_exciter_regs),
            whitebox_mock_exciter_order))) {
        goto fail_create_mock;
    }

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

    whitebox_device->platform_data = WHITEBOX_PLATFORM_DATA(pdev);

    whitebox_gpio_cmx991_reset(whitebox_device->platform_data);

	printk(KERN_INFO "Whitebox: bravo found\n");

    goto done;

fail_gpio_request:
    device_destroy(whitebox_class, whitebox_devno);
fail_create_device:
    cdev_del(&whitebox_device->cdev);
fail_create_cdev:
    unregister_chrdev_region(whitebox_devno, 1);
fail_alloc_region:
    class_destroy(whitebox_class);
fail_create_class:
    whitebox_exciter_destroy(&whitebox_device->mock_exciter.exciter);
fail_create_mock:
    whitebox_exciter_destroy(&whitebox_device->exciter);
fail_create_exciter:
    kfree(whitebox_device);
fail_release_nothing:
done:
    return ret;
}

static int whitebox_remove(struct platform_device* pdev) {
    d_printk(2, "whitebox remove\n");

    whitebox_gpio_free(pdev);

    device_destroy(whitebox_class, whitebox_devno);

    cdev_del(&whitebox_device->cdev);

    unregister_chrdev_region(whitebox_devno, 1);

    class_destroy(whitebox_class);

    whitebox_exciter_destroy(&whitebox_device->mock_exciter.exciter);
    whitebox_exciter_destroy(&whitebox_device->exciter);

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
