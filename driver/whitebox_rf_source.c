#include <linux/dma-mapping.h>

#include "whitebox.h"
#include "whitebox_block.h"

#include "pdma.h"

#define d_printk(level, fmt, args...)				\
	if (whitebox_debug >= level) printk(KERN_INFO "%s: " fmt,	\
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
    d_printk(1, "releasing\n");
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
    d_printk(1, "consumed %zd\n", count);
    rf_source->off += count;
    return rf_source->receiver->ops->consume(rf_source->receiver, count);
}

int whitebox_rf_source_work(struct whitebox_rf_source *rf_source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    size_t count = min(src_count, dest_count);
    dma_addr_t mapping;
    int buf;

    if (count >> 2 == 0)
        return -1;

    mapping = dma_map_single(NULL,
            (void*)src, count, DMA_FROM_DEVICE);
    if (dma_mapping_error(NULL, mapping)) {
        d_printk(0, "failed to map dma\n");
        return -EFAULT;
    }

    if ((buf = pdma_start(rf_source->dma_ch,
            mapping,
            dest,
            count >> 2)) < 0) {
        d_printk(0, "failed to start dma\n");
        return -EFAULT;
    }
    d_printk(1, "started buf=%d src=%08lx dest=%08lx count=%zd\n", buf, src, dest, count);
    rf_source->dma[buf].count = count;
    rf_source->dma[buf].mapping = mapping;
    d_printk(3, "work finish\n");
    return 0;
}

int whitebox_rf_source_work_done(struct whitebox_rf_source *rf_source, int buf)
{
    dma_unmap_single(NULL, rf_source->dma[buf].mapping,
            rf_source->dma[buf].count, DMA_FROM_DEVICE);
    d_printk(1, "finished buf=%d count=%zd\n", buf, rf_source->dma[buf].count);
    return (int)rf_source->dma[buf].count;
}
