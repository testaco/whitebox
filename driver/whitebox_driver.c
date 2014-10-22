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
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/poll.h>
#include <linux/delay.h>

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
int whitebox_debug = WHITEBOX_VERBOSE_DEBUG;
module_param(whitebox_debug, int, S_IRUSR | S_IWUSR);

/*
 * Does a mis-locked PLL cause a file error?
 */
int whitebox_check_plls = 1;
module_param(whitebox_check_plls, int, S_IRUSR | S_IWUSR);

/*
 * Does an overrun or underrun cause a file error?
 */
int whitebox_check_runs = 1;
module_param(whitebox_check_runs, int, S_IRUSR | S_IWUSR);

/*
 * Order of the user read and write circular buffers.
 */
static int whitebox_user_order = 7;
module_param(whitebox_user_order, int, S_IRUSR | S_IWUSR);

/*
 * Whether or not to use the mock.
 */
static int whitebox_mock_en = 1;
module_param(whitebox_mock_en, int, S_IRUSR | S_IWUSR);

/*
 * Order of the mock circular buffer
 */
static int whitebox_mock_order = 0;
module_param(whitebox_mock_order, int, S_IRUSR | S_IWUSR);

/*
 * Block size of dma xfers to the exciter peripheral in bytes.
 */
static int whitebox_exciter_quantum = (3 << 2);
module_param(whitebox_exciter_quantum, int, S_IRUSR | S_IWUSR);

/*
 * Block size of dma xfers to the receiver peripheral.
 */
static int whitebox_receiver_quantum = (3 << 2);
module_param(whitebox_receiver_quantum, int, S_IRUSR | S_IWUSR);

/*
 * Whether or not to automatically turn on the transmit chain when almost full.
 */
static int whitebox_auto_tx = 1;
module_param(whitebox_auto_tx, int, S_IRUSR | S_IWUSR);

/*
 * Offset to add to I & Q coming out of the DUC - for AQM calibration
 */
static int whitebox_tx_i_correction = 0;
module_param(whitebox_tx_i_correction, int, S_IRUSR | S_IWUSR);
static int whitebox_tx_q_correction = 0;
module_param(whitebox_tx_q_correction, int, S_IRUSR | S_IWUSR);

/*
 * Gain to multiply I & Q by coming out of the DUC - for AQM calibration
 */
static int whitebox_tx_i_gain = (int)(1. * WEG_COEFF + 0.5);
module_param(whitebox_tx_i_gain, int, S_IRUSR | S_IWUSR);
static int whitebox_tx_q_gain = (int)(1. * WEG_COEFF + 0.5);
module_param(whitebox_tx_q_gain, int, S_IRUSR | S_IWUSR);

/*
 * Offset to add to I & Q coming into of the DDC - for AQM calibration
 */
static int whitebox_rx_i_correction = 0;
module_param(whitebox_rx_i_correction, int, S_IRUSR | S_IWUSR);
static int whitebox_rx_q_correction = 0;
module_param(whitebox_rx_q_correction, int, S_IRUSR | S_IWUSR);


/*
 * Enable the loopback (for testing)
 */
int whitebox_loopen = 0;
module_param(whitebox_loopen, int, S_IRUSR | S_IWUSR);

/*
 * Buffer delay size.  latency_secs = (threshold / 4) / sample_rate.
 * or threshold = 4 * latency_secs * sample_rate
 */
int whitebox_user_source_buffer_threshold = 3840;  // 20ms @ 48KSPS
module_param(whitebox_user_source_buffer_threshold, int, S_IRUSR | S_IWUSR);
int whitebox_user_sink_buffer_threshold = 3840;  // 20ms @ 48KSPS
module_param(whitebox_user_sink_buffer_threshold, int, S_IRUSR | S_IWUSR);

int whitebox_flow_control = 1;
module_param(whitebox_flow_control, int, S_IRUSR | S_IWUSR);

int whitebox_frame_size = 1024;
module_param(whitebox_frame_size, int, S_IRUSR | S_IWUSR);

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

void d_printk_loop(int level) {
    unsigned long src;
    struct circ_buf *mock_buf = &whitebox_device->mock_buf;
    struct whitebox_user_sink *user_sink = &whitebox_device->user_sink;
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    struct whitebox_rf_sink *rf_sink = &whitebox_device->rf_sink;
    struct whitebox_rf_source *rf_source = &whitebox_device->rf_source;
    u32 exciter_state = rf_sink->exciter->ops->get_state(rf_sink->exciter);
    u32 receiver_state = rf_source->receiver->ops->get_state(rf_source->receiver);

    d_printk(level, "stats %c%c user_source_data/space=%d/%d rf_sink_space=%d mock_data/space=%d/%d rf_source_data=%d user_sink_data/space=%d/%d\n",
        exciter_state & WES_TXEN ? 'T' : ' ',
        receiver_state & WRS_RXEN ? 'R' : ' ',
        whitebox_user_source_data_available(user_source, &src),
        whitebox_user_source_space_available(user_source, &src),
        whitebox_rf_sink_space_available(rf_sink, &src),
        CIRC_CNT_TO_END(mock_buf->head, mock_buf->tail, PAGE_SIZE << whitebox_mock_order),
        CIRC_SPACE_TO_END(mock_buf->head, mock_buf->tail, PAGE_SIZE << whitebox_mock_order),
        whitebox_rf_source_data_available(rf_source, &src),
        whitebox_user_sink_data_available(user_sink, &src),
        whitebox_user_sink_space_available(user_sink, &src));
}

/* Prototpyes */
long whitebox_ioctl_reset(void);

int tx_start(struct whitebox_device *wb);
int tx_exec(struct whitebox_device* wb);
void tx_dma_cb(void *data);
int tx_stop(struct whitebox_device *wb);
int tx_error(struct whitebox_device *wb);

int rx_start(struct whitebox_device *wb);
int rx_exec(struct whitebox_device* wb);
void rx_dma_cb(void *data);
int rx_stop(struct whitebox_device *wb);
int rx_crank(struct whitebox_device *wb, int block);
int rx_error(struct whitebox_device *wb);

static int whitebox_open(struct inode* inode, struct file* filp) {
    int ret = 0;
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    struct whitebox_rf_sink *rf_sink = &whitebox_device->rf_sink;
    struct whitebox_exciter *exciter = whitebox_mock_en ?
            &whitebox_device->mock_exciter.exciter :
            &whitebox_device->exciter;
    struct whitebox_user_sink *user_sink = &whitebox_device->user_sink;
    struct whitebox_rf_source *rf_source = &whitebox_device->rf_source;
    struct whitebox_receiver *receiver = whitebox_mock_en ?
            &whitebox_device->mock_receiver.receiver :
            &whitebox_device->receiver;

    d_printk(2, "whitebox open\n");
    filp->private_data = whitebox_device;

    if (atomic_add_return(1, &use_count) != 1) {
        d_printk(1, "Device in use\n");
        ret = -EBUSY;
        goto fail_in_use;
    }

    atomic_set(&whitebox_device->mapped, 0);
    whitebox_user_source_init(&whitebox_device->user_source,
            whitebox_user_order - 1, &whitebox_device->mapped);

    whitebox_rf_sink_init(&whitebox_device->rf_sink,
            whitebox_device->platform_data->tx_dma_ch,
            tx_dma_cb,
            whitebox_device,
            exciter);

    exciter->quantum = whitebox_exciter_quantum;
    exciter->auto_tx = whitebox_auto_tx;
    exciter->ops->set_threshold(exciter, (u32)((whitebox_exciter_quantum)) |
        ((u32)((WE_FIFO_SIZE - whitebox_exciter_quantum))) << WET_AFVAL_OFFSET);
    exciter->ops->set_correction(exciter,
            (u32)(((s32)whitebox_tx_i_correction & WEC_I_MASK) |
                (((s32)whitebox_tx_q_correction << WEC_Q_OFFSET) & WEC_Q_MASK)));
    exciter->ops->set_gain(exciter,
            (u32)(((u32)whitebox_tx_i_gain & WEG_I_MASK) |
                (((u32)whitebox_tx_q_gain << WEG_Q_OFFSET) & WEG_Q_MASK)));

    receiver->ops->set_correction(receiver,
            (u32)(((s32)whitebox_rx_i_correction & WEC_I_MASK) |
                (((s32)whitebox_rx_q_correction << WEC_Q_OFFSET) & WEC_Q_MASK)));

    ret = whitebox_rf_sink_alloc(rf_sink);
    if (ret < 0) {
        d_printk(0, "DMA Channel request failed\n");
        goto fail_in_use;
    }

    ret = whitebox_user_source_alloc(user_source, whitebox_device->user_buffer);
    if (ret < 0) {
        d_printk(0, "Buffer allocation failed\n");
        goto fail_free_rf_sink;
    }

    whitebox_user_sink_init(&whitebox_device->user_sink,
            whitebox_user_order - 1, &whitebox_device->mapped);

    whitebox_rf_source_init(&whitebox_device->rf_source,
            whitebox_device->platform_data->rx_dma_ch,
            rx_dma_cb,
            whitebox_device,
            receiver);

    receiver->quantum = whitebox_receiver_quantum;

    ret = whitebox_rf_source_alloc(rf_source);
    if (ret < 0) {
        d_printk(0, "DMA Channel request failed\n");
        goto fail_free_tx;
    }

    ret = whitebox_user_sink_alloc(user_sink,  whitebox_device->user_buffer + (PAGE_SIZE << (whitebox_user_order - 1)));
    if (ret < 0) {
        d_printk(0, "Buffer allocation failed\n");
        goto fail_free_rf_source;
    }

    // Reset the stats
    memset(&whitebox_device->tx_stats, 0, sizeof(struct whitebox_stats));
    memset(&whitebox_device->rx_stats, 0, sizeof(struct whitebox_stats));

    // enable dac
    whitebox_gpio_dac_enable(whitebox_device->platform_data);

    whitebox_device->state = WDS_IDLE;

    whitebox_ioctl_reset();

    goto done_open;

fail_free_rf_source:
    whitebox_rf_source_free(rf_source);
fail_free_tx:
    whitebox_user_source_free(user_source);
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
    struct whitebox_user_sink *user_sink = &whitebox_device->user_sink;
    struct whitebox_rf_source *rf_source = &whitebox_device->rf_source;
    d_printk(2, "whitebox release\n");
    
    if (atomic_read(&use_count) != 1) {
        d_printk(0, "Device not in use");
        return -ENOENT;
    }
    if (whitebox_device->state == WDS_TX || whitebox_device->state == WDS_TX_STREAMING) {
        while (tx_stop(whitebox_device) < 0)
            cpu_relax();
    }
    if (whitebox_device->state == WDS_RX || whitebox_device->state == WDS_RX_STREAMING) {
        while (rx_stop(whitebox_device) < 0)
            cpu_relax();
    }

    // Turn off LO
    whitebox_device->adf4351_regs[2] |= WA_PD_MASK;
    whitebox_gpio_adf4351_write(whitebox_device->platform_data,
        whitebox_device->adf4351_regs[2]);

    // Disable DAC
    whitebox_gpio_dac_disable(whitebox_device->platform_data);

    whitebox_rf_sink_free(rf_sink);
    whitebox_user_source_free(user_source);
    whitebox_rf_source_free(rf_source);
    whitebox_user_sink_free(user_sink);
    atomic_dec(&use_count);
    return 0;
}

static void whitebox_mmap_open(struct vm_area_struct *vma) {
    struct whitebox_device *wb = (struct whitebox_device*)vma->vm_private_data;
    d_printk(4, "\n");
    atomic_inc(&wb->mapped);
}

static void whitebox_mmap_close(struct vm_area_struct *vma) {
    struct whitebox_device *wb = (struct whitebox_device*)vma->vm_private_data;
    d_printk(4, "\n");
    atomic_dec(&wb->mapped);
}

static const struct vm_operations_struct whitebox_mmap_ops = {
    .open = whitebox_mmap_open,
    .close = whitebox_mmap_close,
};

static int whitebox_mmap(struct file *filp, struct vm_area_struct *vma)
{
    vma->vm_pgoff = vma->vm_start >> PAGE_SHIFT;
    d_printk(4, "%08lx %08lx %08lx %08lx\n", vma->vm_start, vma->vm_end, vma->vm_pgoff, vma->vm_page_prot);
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
            vma->vm_end - vma->vm_start, vma->vm_page_prot))
        return -EAGAIN;

    //struct inode *inode = filp->f_dentry->d_inode;
    vma->vm_ops = &whitebox_mmap_ops;
    vma->vm_flags |= VM_RESERVED;
    vma->vm_private_data = filp->private_data;
    whitebox_mmap_open(vma);

    return 0;
}

static unsigned long whitebox_get_unmapped_area(struct file* filp,
        unsigned long addr, unsigned long len,
        unsigned long pgoff, unsigned long flags) {
    struct whitebox_device *wb = (struct whitebox_device*)filp->private_data;
    struct whitebox_user_source *user_source = &wb->user_source;
    unsigned long dest;

    d_printk(4, "addr=%08lx len=%08lx pgoff=%08lx flags=%08lx\n", addr, len, pgoff, flags);
    //if (flags & PROT_WRITE) {
        if (len != user_source->buf_size)
            dest = 0;
        if (addr || pgoff)
            dest = 0;
        else
            dest = (unsigned long)user_source->buf.buf;
    //}
    d_printk(4, "dest=%08lx\n", dest);
    return dest;
}

static unsigned int whitebox_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    unsigned long src, dest;

    down(&whitebox_device->sem);

    if (whitebox_device->state == WDS_TX || whitebox_device->state == WDS_TX_STREAMING) {
        if (tx_error(whitebox_device))
            mask |= POLLERR;
    }

    poll_wait(filp, &whitebox_device->write_wait_queue, wait);
    if (whitebox_user_source_space_available(&whitebox_device->user_source, &dest) > 0)
        mask |= POLLOUT | POLLRDNORM;

    if (whitebox_device->state == WDS_RX || whitebox_device->state == WDS_RX_STREAMING) {
        poll_wait(filp, &whitebox_device->read_wait_queue, wait);
        if (whitebox_user_sink_data_available(&whitebox_device->user_sink, &src) > 0)
            mask |= POLLIN | POLLWRNORM;
    }

    if (whitebox_device->state == WDS_IDLE) {
        mask |= POLLOUT | POLLRDNORM | POLLIN | POLLWRNORM;
    }

    up(&whitebox_device->sem);

    return mask;
}

static int whitebox_read(struct file* filp, char __user* buf, size_t count, loff_t* pos) {
    unsigned long src;
    size_t src_count = 0;
    int ret = 0, err = 0;
    struct whitebox_user_sink *user_sink = &whitebox_device->user_sink;
    
    d_printk(1, "whitebox read decim=%d\n",
            whitebox_device->receiver.ops->get_decim(&whitebox_device->receiver));
    d_printk_loop(4);

    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }

    if (whitebox_device->state == WDS_TX || whitebox_device->state == WDS_TX_STREAMING || whitebox_device->state == WDS_TX_STOPPING) {
        up(&whitebox_device->sem);
        d_printk(2, "in transmit\n");
        return -EBUSY;
    }

    if (count == 0) {
        up(&whitebox_device->sem);
        return 0;
    }

    d_printk_loop(4);

    if (whitebox_device->state == WDS_IDLE) {
        whitebox_device->state = WDS_RX_STREAMING;
        up(&whitebox_device->sem);
        d_printk(1, "rx_crank block=%d\n", !(filp->f_flags & O_NONBLOCK));
        rx_crank(whitebox_device, !(filp->f_flags & O_NONBLOCK));
    }
    else if (whitebox_device->state == WDS_RX_STREAMING) {
        up(&whitebox_device->sem);
        rx_exec(whitebox_device);
    }

    d_printk(3, "going to wait for data\n");

    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }

    while (!(err = rx_error(whitebox_device)) && ((src_count = whitebox_user_sink_data_available(user_sink, &src)) < count)) {
        up(&whitebox_device->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;

        d_printk(2, "RX_EXEC waiting %d\n", count);
        rx_exec(whitebox_device);
        d_printk_loop(3);

        if (wait_event_interruptible(whitebox_device->read_wait_queue,
                (((src_count = whitebox_user_sink_data_available(user_sink, &src)) >= count) || (err = rx_error(whitebox_device)))))
            return -ERESTARTSYS;
        if (down_interruptible(&whitebox_device->sem))
            return -ERESTARTSYS;

        d_printk(4, "waiting done\n");
    }

    if (err) {
        up(&whitebox_device->sem);
        d_printk(1, "rx_error=%d user_sink_data=%zd\n", err, src_count);
        return -EIO;
    }

    ret = whitebox_user_sink_work(user_sink, src, src_count, (unsigned long)buf, count);

    if (ret < 0) {
        up(&whitebox_device->sem);
        return -EFAULT;
    }

    whitebox_user_sink_consume(user_sink, ret);

    up(&whitebox_device->sem);

    return ret;
}

static int whitebox_write(struct file* filp, const char __user* buf, size_t count, loff_t* pos) {
    unsigned long dest;
    size_t dest_count;
    int ret = 0, err = 0;
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    
    d_printk(2, "whitebox write\n");

    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }
    if (whitebox_device->state == WDS_RX || whitebox_device->state == WDS_RX_STREAMING || whitebox_device->state == WDS_RX_STOPPING) {
        up(&whitebox_device->sem);
        d_printk(1, "in receive\n");
        d_printk_loop(4);
        return -EBUSY;
    }
    if (whitebox_device->state == WDS_IDLE) {
        whitebox_device->state = WDS_TX;
        tx_start(whitebox_device);
    }
    if (count == 0) {
        up(&whitebox_device->sem);
        return 0;
    }
    if ((ret = tx_error(whitebox_device))) {
        dest_count = whitebox_user_source_data_available(user_source, &dest);
        d_printk(2, "tx_error=%d user_source_data=%zd\n", ret, dest_count);
        up(&whitebox_device->sem);
        return -EIO;
    }

    while (((dest_count = whitebox_user_source_space_available(user_source, &dest)) < count) && !(err = tx_error(whitebox_device))) {
        up(&whitebox_device->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (wait_event_interruptible(whitebox_device->write_wait_queue,
                (((dest_count = whitebox_user_source_space_available(user_source, &dest)) >= count) || (err = tx_error(whitebox_device)))))
            return -ERESTARTSYS;
        if (down_interruptible(&whitebox_device->sem))
            return -ERESTARTSYS;
    }

    if (err) {
        d_printk(1, "tx_error=%d user_source_data=%zd\n", err, dest_count);
        up(&whitebox_device->sem);
        return -EIO;
    }

    d_printk_loop(4);

    ret = whitebox_user_source_work(user_source, (unsigned long)buf, count, dest, dest_count);

    if (ret < 0) {
        up(&whitebox_device->sem);
        return -EFAULT;
    }

    whitebox_user_source_produce(user_source, ret);

    d_printk_loop(4);

    up(&whitebox_device->sem);

    d_printk_loop(4);

    if (whitebox_device->state == WDS_TX_STREAMING) {
        tx_exec(whitebox_device);
    } else {
        d_printk(1, "so here we are and use source data is %d\n",
            whitebox_user_source_data_total(user_source));
        if (whitebox_user_source_data_total(user_source) >=
                whitebox_user_source_buffer_threshold) {
            d_printk(1, "streaming %d %d\n",
                whitebox_user_source_data_total(user_source),
                whitebox_user_source_buffer_threshold);
            whitebox_device->state = WDS_TX_STREAMING;
            tx_exec(whitebox_device);
        }
    }

    d_printk_loop(4);

    return ret;
}

static int whitebox_fsync(struct file *file, struct dentry *dentry, int datasync)
{
    struct whitebox_user_source *user_source = &whitebox_device->user_source;
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    unsigned long src;
    size_t src_count;
    u32 state;
    int err = 0;
    if (down_interruptible(&whitebox_device->sem)) {
        return -ERESTARTSYS;
    }
    d_printk(1, "state=%d\n", whitebox_device->state);
    if (whitebox_device->state == WDS_TX || whitebox_device->state == WDS_TX_STREAMING) {
        state = exciter->ops->get_state(exciter);
        d_printk(1, "txen=%s\n", state & WES_TXEN ? "true" : "false");
        whitebox_device->state = WDS_TX_STOPPING;
        d_printk(1, "draining\n");
        if (!(state & WES_TXEN))
            exciter->ops->set_state(exciter, WES_TXEN);

        while (((src_count = whitebox_user_source_data_available(user_source, &src)) > 0) && !(err = tx_error(whitebox_device))) {
            d_printk(1, "upping\n");
            up(&whitebox_device->sem);

            d_printk(1, "waiting for user source to drain threshold=%d count=%d\n", whitebox_user_source_buffer_threshold, src_count);

            tx_exec(whitebox_device);

            d_printk_loop(1);

            if (down_interruptible(&whitebox_device->sem))
                return -ERESTARTSYS;
            else
                d_printk(1, "downing, no interrupt\n");
        }

        if (err) {
            d_printk(1, "wait for user_source to drain error tx_error=%d user_source_data=%zd\n", err, src_count);
            up(&whitebox_device->sem);
            return -EIO;
        }

        d_printk(1, "stopping tx %d\n", exciter->ops->get_state(exciter) & WES_TXEN);

        while ((tx_stop(whitebox_device) < 0) && !(err = tx_error(whitebox_device))) {
            state = exciter->ops->get_state(exciter);
            d_printk(1, "waiting for tx to stop %d\n", state & WES_TXEN);
        }

        if (err) {
            d_printk(1, "wait for tx to stop tx_error=%d user_source_data=%zd\n", err, src_count);
            up(&whitebox_device->sem);
            return -EIO;
        }

        d_printk(1, "stopped\n");

        while ((exciter->ops->get_state(exciter) & WES_TXEN) && !(err = tx_error(whitebox_device))) {
            up(&whitebox_device->sem);

            d_printk(1, "waiting for dma & hardware dsp flow to finish\n");

            if (down_interruptible(&whitebox_device->sem))
                return -ERESTARTSYS;
        }

        if (err) {
            d_printk(1, "wait for tx_en to shut off error tx_error=%d user_source_data=%zd\n", err, src_count);
            up(&whitebox_device->sem);
            return -EIO;
        }

        whitebox_device->state = WDS_IDLE;
    }

    if (whitebox_device->state == WDS_RX || whitebox_device->state == WDS_RX_STREAMING) {
        whitebox_device->state = WDS_RX_STOPPING;

        rx_stop(whitebox_device);

        whitebox_device->state = WDS_IDLE;
    }

    up(&whitebox_device->sem);
    return 0;
}

long whitebox_ioctl_reset(void) {
    int i;
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    struct whitebox_receiver *receiver = whitebox_device->rf_source.receiver;
    whitebox_gpio_cmx991_reset(whitebox_device->platform_data);
    for (i = 0; i < WA_REGS_COUNT; ++i) {
        whitebox_device->adf4351_regs[i] = 0;
    }
    for (i = 0; i < WC_REGS_COUNT; ++i) {
        whitebox_device->cmx991_regs[i] = -1;
    }
    exciter->ops->set_state(exciter, WS_CLEAR);
    receiver->ops->set_state(receiver, WS_CLEAR);
    return 0;
}

long whitebox_ioctl_locked(void) {
    u8 c;
    u8 locked;
#if WC_USE_PLL
    c = whitebox_gpio_cmx991_read(whitebox_device->platform_data,
        WHITEBOX_CMX991_LD_REG);
#else
    c = WHITEBOX_CMX991_LD_REG;
#endif
    locked = whitebox_gpio_adf4351_locked(whitebox_device->platform_data)
            && (c & WHITEBOX_CMX991_LD_MASK);
    return locked;
}

long whitebox_ioctl_exciter_clear(void) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    pdma_clear(whitebox_device->platform_data->tx_dma_ch);
    exciter->ops->set_state(exciter, WS_CLEAR);
    return 0;
}

long whitebox_ioctl_exciter_get(unsigned long arg) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_args_t w;
    u16 o, u;
    unsigned long src;
    w.flags.exciter.state = exciter->ops->get_state(exciter);
    w.flags.exciter.interp = exciter->ops->get_interp(exciter);
    w.flags.exciter.fcw = exciter->ops->get_fcw(exciter);
    exciter->ops->get_runs(exciter, &o, &u);
    w.flags.exciter.runs = ((u32)o << WER_OVERRUNS_OFFSET) | (u32)u;
    w.flags.exciter.threshold = exciter->ops->get_threshold(exciter);
    w.flags.exciter.correction = exciter->ops->get_correction(exciter);
    w.flags.exciter.gain = exciter->ops->get_gain(exciter);
    w.flags.exciter.available = exciter->ops->space_available(exciter, &src);
    w.flags.exciter.debug = exciter->ops->get_debug(exciter);
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
    exciter->ops->set_correction(exciter, w.flags.exciter.correction);
    exciter->ops->set_gain(exciter, w.flags.exciter.gain);
    return 0;
}

long whitebox_ioctl_exciter_clear_mask(unsigned long arg) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;
    exciter->ops->clear_state(exciter, w.flags.exciter.state);
    return 0;
}

long whitebox_ioctl_receiver_clear(void) {
    struct whitebox_receiver *receiver = whitebox_device->rf_source.receiver;
    pdma_clear(whitebox_device->platform_data->rx_dma_ch);
    receiver->ops->set_state(receiver, WS_CLEAR);
    return 0;
}

long whitebox_ioctl_receiver_get(unsigned long arg) {
    struct whitebox_receiver *receiver = whitebox_device->rf_source.receiver;
    whitebox_args_t w;
    unsigned long dest;
    w.flags.receiver.state = receiver->ops->get_state(receiver);
    w.flags.receiver.decim = receiver->ops->get_decim(receiver);
    w.flags.receiver.fcw = receiver->ops->get_fcw(receiver);
    // TODO w.flags.receiver.runs = receiver->ops->get_runs(receiver);
    w.flags.receiver.threshold = receiver->ops->get_threshold(receiver);
    w.flags.receiver.correction = receiver->ops->get_correction(receiver);
    w.flags.receiver.available = receiver->ops->data_available(receiver, &dest);
    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_receiver_set(unsigned long arg) {
    struct whitebox_receiver *receiver = whitebox_device->rf_source.receiver;
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;
    receiver->ops->set_state(receiver, w.flags.receiver.state);
    receiver->ops->set_decim(receiver, w.flags.receiver.decim);
    receiver->ops->set_fcw(receiver, w.flags.receiver.fcw);
    receiver->ops->set_threshold(receiver, w.flags.receiver.threshold);
    receiver->ops->set_correction(receiver, w.flags.receiver.correction);
    return 0;
}

long whitebox_ioctl_receiver_clear_mask(unsigned long arg) {
    struct whitebox_receiver *receiver = whitebox_device->rf_source.receiver;
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;
    receiver->ops->clear_state(receiver, w.flags.receiver.state);
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

    for (i = 0; i < WC_REGS_COUNT; ++i) {
        if (whitebox_device->cmx991_regs[i] != w.flags.cmx991[i]) {
            d_printk(3, "setting %d to %08x\n", whitebox_cmx991_regs_write_lut[i], w.flags.cmx991[i]);
            whitebox_device->cmx991_regs[i] = w.flags.cmx991[i];
            whitebox_gpio_cmx991_write(whitebox_device->platform_data, 
                    whitebox_cmx991_regs_write_lut[i],
                    w.flags.cmx991[i]);
        }
    }

    return 0;
}

long whitebox_ioctl_cmx991_locked(void) {

    return whitebox_gpio_cmx991_read(whitebox_device->platform_data,
        WHITEBOX_CMX991_LD_REG) & WHITEBOX_CMX991_LD_MASK;
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
        if (whitebox_device->adf4351_regs[i] != w.flags.adf4351[i]) {
            whitebox_device->adf4351_regs[i] = w.flags.adf4351[i];
            whitebox_gpio_adf4351_write(whitebox_device->platform_data, 
                    w.flags.adf4351[i]);
        }
    }

    return 0;
}

long whitebox_ioctl_adf4351_locked(void) {
    return whitebox_gpio_adf4351_locked(whitebox_device->platform_data);
}

long whitebox_ioctl_mock_command(unsigned long arg) {
    whitebox_args_t w;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;

    if (w.mock_command == WMC_CAUSE_UNDERRUN) {
        WHITEBOX_EXCITER(&whitebox_device->mock_exciter.exciter)->runs +=
                0x00010000;
    }
    if (w.mock_command == WMC_CAUSE_OVERRUN) {
        d_printk(1, "causing overrun\n");
        WHITEBOX_RECEIVER(&whitebox_device->mock_receiver.receiver)->runs +=
                0x00010000;
    }

    return 0;
}

long whitebox_ioctl_mmap_write(unsigned long arg)
{
    long count;
    unsigned long dest;
    count = whitebox_user_source_space_available(&whitebox_device->user_source, &dest);

    // TODO: only send how many samples need to go for the interval and
    // sample rate.
    if (whitebox_flow_control) {
        if (whitebox_user_source_data_total(&whitebox_device->user_source) >= whitebox_user_source_buffer_threshold)
            return 0;
    }

    if (copy_to_user((unsigned long*)arg, &dest,
            sizeof(unsigned long)))
        return -EACCES;
    
    return count;
}

long whitebox_ioctl_mmap_read(unsigned long arg)
{
    long count;
    unsigned long src;
    int err;

    if (down_interruptible(&whitebox_device->sem))
        return -ERESTARTSYS;

    if (whitebox_device->state == WDS_IDLE) {
        whitebox_device->state = WDS_RX_STREAMING;
        up(&whitebox_device->sem);
        d_printk(1, "rx_crank\n");
        rx_crank(whitebox_device, 0);
    } else if (whitebox_device->state == WDS_RX_STREAMING) {
        up(&whitebox_device->sem);
        if (!(whitebox_device->receiver.ops->get_state(&whitebox_device->receiver) & WRS_RXEN)) {
            BUG();
        }
        d_printk(1, "stream\n");
        rx_exec(whitebox_device);
    } else {
        up(&whitebox_device->sem);
        d_printk(1, "bad bad bad");
        return -EBADFD;
    }

    err = rx_error(whitebox_device);
    if (err)
        return -EIO;

    count = whitebox_user_sink_data_available(&whitebox_device->user_sink, &src);

    if (copy_to_user((unsigned long*)arg, &src,
            sizeof(unsigned long)))
        return -EACCES;

    return count;
}

long whitebox_ioctl_fir_get(unsigned long arg) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_args_t w;
    u32 fir = exciter->ops->get_fir(exciter);
    u8 i;
    w.flags.fir.bank = (fir & 0x180) >> 7;
    w.flags.fir.n = fir & 0x7f;
    exciter->ops->set_fir(exciter, fir | WF_ACCESS_COEFFS);
    for (i = 0; i < w.flags.fir.n; ++i)
        w.flags.fir.coeff[i] = exciter->ops->get_fir_coeff(exciter, i);

    if (copy_to_user((whitebox_args_t*)arg, &w,
            sizeof(whitebox_args_t)))
        return -EACCES;
    return 0;
}

long whitebox_ioctl_fir_set(unsigned long arg) {
    struct whitebox_exciter *exciter = whitebox_device->rf_sink.exciter;
    whitebox_args_t w;
    u32 fir;
    int i;
    if (copy_from_user(&w, (whitebox_args_t*)arg,
            sizeof(whitebox_args_t)))
        return -EACCES;
    fir = ((w.flags.fir.bank & 0x3) << 7) | (w.flags.fir.n & 0x7f);

    exciter->ops->set_fir(exciter, fir | WF_ACCESS_COEFFS);
    for (i = 0; i < w.flags.fir.n; ++i)
        exciter->ops->set_fir_coeff(exciter, i, w.flags.fir.coeff[i]);

    return 0;
}

static long whitebox_ioctl(struct file* filp, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case W_RESET:
            return whitebox_ioctl_reset();
        case W_LOCKED:
            return whitebox_ioctl_locked();
        case WE_CLEAR:
            return whitebox_ioctl_exciter_clear();
        case WE_GET:
            return whitebox_ioctl_exciter_get(arg);
        case WE_SET:
            return whitebox_ioctl_exciter_set(arg);
        case WE_CLEAR_MASK:
            return whitebox_ioctl_exciter_clear_mask(arg);
        case WR_CLEAR:
            return whitebox_ioctl_receiver_clear();
        case WR_GET:
            return whitebox_ioctl_receiver_get(arg);
        case WR_SET:
            return whitebox_ioctl_receiver_set(arg);
        case WR_CLEAR_MASK:
            return whitebox_ioctl_receiver_clear_mask(arg);
        case WC_GET:
            return whitebox_ioctl_cmx991_get(arg);
        case WC_SET:
            return whitebox_ioctl_cmx991_set(arg);
        case WC_LOCKED:
            return whitebox_ioctl_cmx991_locked();
        case WA_GET:
            return whitebox_ioctl_adf4351_get(arg);
        case WA_SET:
            return whitebox_ioctl_adf4351_set(arg);
        case WA_LOCKED:
            return whitebox_ioctl_adf4351_locked();
        case WM_CMD:
            return whitebox_ioctl_mock_command(arg);
        case W_MMAP_WRITE:
            return whitebox_ioctl_mmap_write(arg);
        case W_MMAP_READ:
            return whitebox_ioctl_mmap_read(arg);
        case WF_GET:
            return whitebox_ioctl_fir_get(arg);
        case WF_SET:
            return whitebox_ioctl_fir_set(arg);
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
    .mmap = whitebox_mmap,
    .get_unmapped_area = whitebox_get_unmapped_area,
    .poll = whitebox_poll,
};

void stats_show(struct seq_file *m, struct whitebox_stats *stats) {
    int i;
    seq_printf(m, "bytes=%ld\n", stats->bytes);
    seq_printf(m, "bytes_per_call=[");
    for (i = 0; i < W_EXEC_DETAIL_COUNT; ++i) {
        int offset = (stats->exec_detail_index + i) & (W_EXEC_DETAIL_COUNT - 1);
        struct whitebox_stats_exec_detail *detail = &stats->exec_detail[offset];
        
        if (detail->time > 0)
            seq_printf(m, "(%d, %zu, %zu, %d, %d), ", detail->time, detail->src, detail->dest, detail->bytes, detail->result);
    }
    seq_printf(m, "]\n");

    seq_printf(m, "exec_calls=%ld\n", stats->exec_calls);
    seq_printf(m, "exec_busy=%ld\n", stats->exec_busy);
    seq_printf(m, "exec_nop_src=%ld\n", stats->exec_nop_src);
    seq_printf(m, "exec_nop_dest=%ld\n", stats->exec_nop_dest);
    seq_printf(m, "exec_failed=%ld\n", stats->exec_failed);
    seq_printf(m, "exec_success_slow=%ld\n", stats->exec_success_slow);
    seq_printf(m, "exec_dma_start=%ld\n", stats->exec_dma_start);
    seq_printf(m, "exec_dma_finished=%ld\n", stats->exec_dma_finished);
    seq_printf(m, "stop=%ld\n", stats->stop);
    seq_printf(m, "error=%ld\n", stats->error);
    seq_printf(m, "last_error=%d\n", stats->last_error);
}

static int tx_stats_show(struct seq_file *m, void *private)
{
    struct whitebox_device *wb = (struct whitebox_device*)m->private;
    struct whitebox_stats *stats = &wb->tx_stats;

    stats_show(m, stats);

    return 0;
}

static int tx_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, tx_stats_show, inode->i_private);
}

static const struct file_operations tx_stats_fops = {
    .owner = THIS_MODULE,
    .open = tx_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int rx_stats_show(struct seq_file *m, void *private)
{
    struct whitebox_device *wb = (struct whitebox_device*)m->private;
    struct whitebox_stats *stats = &wb->rx_stats;

    stats_show(m, stats);

    return 0;
}

static int rx_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, rx_stats_show, inode->i_private);
}

static const struct file_operations rx_stats_fops = {
    .owner = THIS_MODULE,
    .open = rx_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int whitebox_probe(struct platform_device* pdev) {
    struct resource* whitebox_exciter_regs;
    struct resource* whitebox_receiver_regs;
    //int irq;
    struct device* dev;
    int ret = 0;

    d_printk(2, "whitebox probe\n");

    whitebox_exciter_regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!whitebox_exciter_regs) {
        d_printk(0, "no register base for Whitebox exciter\n");
        ret = -ENXIO;
        goto fail_release_nothing;
    }

    whitebox_receiver_regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (!whitebox_receiver_regs) {
        d_printk(0, "no register base for Whitebox receiver\n");
        ret = -ENXIO;
        goto fail_release_nothing;
    }

    /*irq = platform_get_irq(pdev, 0);
    if (irq < 0) {
        d_printk(0, "invalid IRQ%d\n", irq);
        ret = -ENXIO;
        goto fail_release_nothing;
    }*/

    whitebox_device = kzalloc(sizeof(struct whitebox_device), GFP_KERNEL);

    sema_init(&whitebox_device->sem, 1);

    whitebox_device->debugfs_root = debugfs_create_dir("whitebox", NULL);
    if (!whitebox_device->debugfs_root) {
        d_printk(0, "cannot create debugfs dir\n");
        ret = -ENXIO;
        goto fail_debugfs_create_dir;
    }

    debugfs_create_file("tx_stats", S_IRUGO, whitebox_device->debugfs_root,
                        whitebox_device, &tx_stats_fops);
    debugfs_create_file("rx_stats", S_IRUGO, whitebox_device->debugfs_root,
                        whitebox_device, &rx_stats_fops);

    init_waitqueue_head(&whitebox_device->write_wait_queue);
    init_waitqueue_head(&whitebox_device->read_wait_queue);

    whitebox_device->mock_buf.buf = (char*)
            __get_free_pages(GFP_KERNEL | __GFP_DMA | __GFP_COMP |
            __GFP_NOWARN, whitebox_mock_order);
    if (!whitebox_device->mock_buf.buf) {
        d_printk(0, "failed to create mock buffer\n");
        goto fail_create_mock_buf;
    }

    if ((ret = whitebox_exciter_create(&whitebox_device->exciter,
            whitebox_exciter_regs->start, 
            resource_size(whitebox_exciter_regs)))) {
        goto fail_create_exciter;
    }

    if ((ret = whitebox_mock_exciter_create(&whitebox_device->mock_exciter,
            resource_size(whitebox_exciter_regs),
            whitebox_mock_order,
            &whitebox_device->mock_buf))) {
        goto fail_create_mock_exciter;
    }

    if ((ret = whitebox_receiver_create(&whitebox_device->receiver,
            whitebox_receiver_regs->start, 
            resource_size(whitebox_receiver_regs)))) {
        goto fail_create_receiver;
    }

    if ((ret = whitebox_mock_receiver_create(&whitebox_device->mock_receiver,
            resource_size(whitebox_receiver_regs),
            whitebox_mock_order,
            &whitebox_device->mock_buf))) {
        goto fail_create_mock_receiver;
    }

    //whitebox_device->irq = irq;
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

    whitebox_device->user_buffer =
            __get_free_pages(GFP_KERNEL | __GFP_DMA | __GFP_COMP |
                __GFP_NOWARN, whitebox_user_order);

    if (!whitebox_device->user_buffer) {
        goto fail_gpio_request;
    }

	printk(KERN_INFO "Whitebox bravo mapped to address 0x%08lx\n", (unsigned long)whitebox_exciter_regs->start);

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
    whitebox_receiver_destroy(&whitebox_device->mock_receiver.receiver);
fail_create_mock_receiver:
    whitebox_receiver_destroy(&whitebox_device->receiver);
fail_create_receiver:
    whitebox_exciter_destroy(&whitebox_device->mock_exciter.exciter);
fail_create_mock_exciter:
    whitebox_exciter_destroy(&whitebox_device->exciter);
fail_create_exciter:
    free_pages((unsigned long)whitebox_device->mock_buf.buf,
            whitebox_mock_order);
fail_create_mock_buf:
    debugfs_remove_recursive(whitebox_device->debugfs_root);
fail_debugfs_create_dir:
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

    whitebox_receiver_destroy(&whitebox_device->mock_receiver.receiver);
    whitebox_receiver_destroy(&whitebox_device->receiver);
    whitebox_exciter_destroy(&whitebox_device->mock_exciter.exciter);
    whitebox_exciter_destroy(&whitebox_device->exciter);

    debugfs_remove_recursive(whitebox_device->debugfs_root);

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
        .start = WHITEBOX_RECEIVER_REGS,
        .end = WHITEBOX_RECEIVER_REGS + WHITEBOX_RECEIVER_REGS_COUNT,
        .flags = IORESOURCE_MEM,
    },
};

/*
 * These whitebox pin to Linux kernel GPIO mappings are derived from the
 * Whitebox Libero SmartDesign, as is the DMA channel allocations.
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
    .rx_dma_ch          = 1,
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
