#include <linux/dma-mapping.h>

#include "whitebox.h"
#include "whitebox_block.h"

#include "pdma.h"

static int whitebox_rf_source_debug = WHITEBOX_VERBOSE_DEBUG;
#define d_printk(level, fmt, args...)				\
	if (whitebox_rf_source_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void whitebox_rf_source_init(struct whitebox_rf_source *rf_source,
        int dma_ch, void (*dma_cb)(void*), void *dma_cb_data,
        struct whitebox_receiver *receiver)
{
    spin_lock_init(&rf_source->lock);
    rf_source->off = 0;
    rf_source->dma_ch = dma_ch;
    rf_source->dma_cb = dma_cb;
    rf_source->dma_cb_data = dma_cb_data;
    rf_source->receiver = receiver;
}

int whitebox_rf_source_alloc(struct whitebox_rf_source *rf_source)
{
    return pdma_request(rf_source->dma_ch,
            (pdma_irq_handler_t)rf_source->dma_cb,
            rf_source->dma_cb_data,
            10,
            rf_source->receiver->pdma_config);
}

void whitebox_rf_source_free(struct whitebox_rf_source *rf_source)
{
    pdma_release(rf_source->dma_ch);
}

size_t whitebox_rf_source_data_available(struct whitebox_rf_source *rf_source,
        unsigned long *dest)
{
    size_t count;
    if (pdma_buffers_available(rf_source->dma_ch) > 0)
        count = rf_source->receiver->ops->data_available(rf_source->receiver, dest);
    else
        count = 0;
    d_printk(3, "%d\n", count);
    return count;
}

int whitebox_rf_source_consume(struct whitebox_rf_source *rf_source, size_t count)
{
    rf_source->off += count;
    return rf_source->receiver->ops->consume(rf_source->receiver, count);
}

int whitebox_rf_source_work(struct whitebox_rf_source *rf_source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    rf_source->dma_count = min(src_count, dest_count) & ~3;
    if (rf_source->dma_count == 0)
        return 0;

    d_printk(1, "src=%08lx src_count=%zu dest=%08lx dest_count=%zu\n",
            src, src_count, dest, dest_count);

    rf_source->dma_mapping = dma_map_single(NULL,
            (void*)src, rf_source->dma_count, DMA_FROM_DEVICE);
    if (dma_mapping_error(NULL, rf_source->dma_mapping)) {
        d_printk(0, "failed to map dma\n");
        return -EFAULT;
    }

    pdma_start(rf_source->dma_ch,
            rf_source->dma_mapping,
            dest,
            rf_source->dma_count >> 2);
    return 0;
}

int whitebox_rf_source_work_done(struct whitebox_rf_source *rf_source)
{
    dma_unmap_single(NULL, rf_source->dma_mapping,
            rf_source->dma_count, DMA_FROM_DEVICE);
    return (int)rf_source->dma_count;
}
