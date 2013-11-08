#include <linux/dma-mapping.h>

#include "whitebox.h"
#include "whitebox_block.h"

#include "pdma.h"

static int whitebox_rf_sink_debug = 0;
#define d_printk(level, fmt, args...)				\
	if (whitebox_rf_sink_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

u32 whitebox_rf_sink_get_state(struct whitebox_rf_sink *rf_sink)
{
    u32 state;
    struct whitebox_device *wb = container_of(rf_sink, struct whitebox_device, rf_sink); 
    state = WHITEBOX_EXCITER(wb)->state;
    state = WHITEBOX_EXCITER(wb)->state;
    return state;
}

void whitebox_rf_sink_set_state(struct whitebox_rf_sink *rf_sink, u32 state_mask)
{
    u32 state;
    struct whitebox_device *wb = container_of(rf_sink, struct whitebox_device, rf_sink); 
    state = WHITEBOX_EXCITER(wb)->state;
    state = WHITEBOX_EXCITER(wb)->state;
    WHITEBOX_EXCITER(wb)->state = state | state_mask;
}

void whitebox_rf_sink_clear_state(struct whitebox_rf_sink *rf_sink, u32 state_mask)
{
    u32 state;
    struct whitebox_device *wb = container_of(rf_sink, struct whitebox_device, rf_sink); 
    state = WHITEBOX_EXCITER(wb)->state;
    state = WHITEBOX_EXCITER(wb)->state;
    WHITEBOX_EXCITER(wb)->state = state & ~state_mask;
}

struct whitebox_rf_sink_operations whitebox_rf_sink_ops = {
    .get_state = whitebox_rf_sink_get_state,
    .set_state = whitebox_rf_sink_set_state,
    .clear_state = whitebox_rf_sink_clear_state,
};

void whitebox_rf_sink_init(struct whitebox_rf_sink *rf_sink,
        unsigned long regs_start, size_t regs_size,
        int dma_ch, void (*dma_cb)(void*), void *dma_cb_data,
        size_t quantum)
{
    spin_lock_init(&rf_sink->lock);
    rf_sink->off = 0;
    rf_sink->regs_start = regs_start;
    rf_sink->regs_size = regs_size;
    rf_sink->dma_ch = dma_ch;
    rf_sink->dma_cb = dma_cb;
    rf_sink->dma_cb_data = dma_cb_data;
    rf_sink->quantum = quantum;
    rf_sink->rf_ops = &whitebox_rf_sink_ops;
}

int whitebox_rf_sink_alloc(struct whitebox_rf_sink *rf_sink)
{
    rf_sink->regs = ioremap(rf_sink->regs_start, rf_sink->regs_size);
    if (!rf_sink->regs) {
        d_printk(0, "unable to map registers for "
            "whitebox exciter base=%08lx\n", rf_sink->regs_start);
        return -EINVAL;
    }
    return pdma_request(rf_sink->dma_ch,
            (pdma_irq_handler_t)rf_sink->dma_cb,
            rf_sink->dma_cb_data,
            10,
            PDMA_CONTROL_PER_SEL_FPGA0 |
            PDMA_CONTROL_HIGH_PRIORITY |
            PDMA_CONTROL_XFER_SIZE_4B |
            PDMA_CONTROL_DST_ADDR_INC_0 |
            PDMA_CONTROL_SRC_ADDR_INC_4 |
            PDMA_CONTROL_PERIPH |
            PDMA_CONTROL_DIR_MEM_TO_PERIPH |
            PDMA_CONTROL_INTEN);
}

void whitebox_rf_sink_free(struct whitebox_rf_sink *rf_sink)
{
    pdma_release(rf_sink->dma_ch);
    iounmap(rf_sink->regs);
}

size_t whitebox_rf_sink_space_available(struct whitebox_rf_sink *rf_sink,
        unsigned long *dest)
{
    u32 state;
    state = rf_sink->rf_ops->get_state(rf_sink);
    if (state & WES_AFULL) {
        if (!(state & WES_TXEN)) {
            rf_sink->rf_ops->set_state(rf_sink, WES_TXEN);
            d_printk(1, "afull, txen\n");
        }
        return 0;
    }
    return rf_sink->quantum;
}

int whitebox_rf_sink_produce(struct whitebox_rf_sink *rf_sink, size_t count)
{
    rf_sink->off += count;
    return 0;
}

int whitebox_rf_sink_work(struct whitebox_rf_sink *rf_sink,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    rf_sink->dma_count = min(src_count, dest_count);
    rf_sink->dma_mapping = dma_map_single(NULL,
            (void*)src, rf_sink->dma_count, DMA_TO_DEVICE);
    if (dma_mapping_error(NULL, rf_sink->dma_mapping)) {
        d_printk(0, "failed to map dma\n");
        return -EFAULT;
    }

    pdma_start(rf_sink->dma_ch,
            rf_sink->dma_mapping,
            dest,
            rf_sink->dma_count >> 2);
    return 0;
}

int whitebox_rf_sink_work_done(struct whitebox_rf_sink *rf_sink)
{
    dma_unmap_single(NULL, rf_sink->dma_mapping,
            rf_sink->dma_count, DMA_TO_DEVICE);
    return (int)rf_sink->dma_count;
}
