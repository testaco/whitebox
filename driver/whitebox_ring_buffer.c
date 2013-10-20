#include <asm/bug.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "whitebox_ring_buffer.h"

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int whitebox_ring_buffer_debug = 0;

/*
 * User can change verbosity of the driver
 */
module_param(whitebox_ring_buffer_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(whitebox_ring_buffer_debug, "whitebox ring buffer debugging level, >0 is verbose");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (whitebox_ring_buffer_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void d_printk_rb(int level, struct whitebox_ring_buffer* rb) {
    int i;
    char* flags = "NONE";
    struct ring_buffer_page_mapping* mapping;
    
    d_printk(level, "num=%d avail=%d write=%d read=%d\n", rb->num_pages,
            rb->readable_pages, rb->write_page, rb->read_page);
    for(i = 0; i < rb->num_pages; ++i) {
        mapping = &rb->page_mappings[i];
        if (mapping->flags & RB_KERNEL)
            flags = "KERNEL";
        if (mapping->flags & RB_USER)
            flags = "USER";
        if (mapping->flags & RB_DMA)
            flags = "DMA";
        d_printk(level, "page=%d flags=%s\n", i, flags);
    }
}

int whitebox_ring_buffer_alloc(struct whitebox_ring_buffer* rb, unsigned num_pages) {
    int ret = 0;
    int i;
    d_printk(2, "num_pages=%d \n", num_pages);
    rb->num_pages = num_pages;
    rb->page_mappings =
            kzalloc(sizeof(struct ring_buffer_page_mapping) * rb->num_pages,
            GFP_KERNEL);
    if (!(rb->page_mappings)) {
        d_printk(0, "failed allocating page mappings\n");
        ret = -ENOMEM;
        goto fail_alloc_page_struct;
    }

    for (i = 0; i < rb->num_pages; ++i) {
        struct ring_buffer_page_mapping* mapping = &rb->page_mappings[i];
        mapping->page = __get_free_page(GFP_KERNEL | __GFP_DMA |
                __GFP_COMP | __GFP_ZERO | __GFP_NOWARN);
        if(!(mapping->page)) {
            d_printk(0, "out of memory for pages\n");
            ret = -ENOMEM;
            goto fail_alloc_pages;
        }
    }

    spin_lock_init(&rb->lock);

    goto done;

fail_alloc_pages:
    for (i = 0; i < rb->num_pages; ++i) {
        free_page(rb->page_mappings[i].page);
        rb->page_mappings[i].page = 0;
    }

    kfree(rb->page_mappings);
fail_alloc_page_struct:
done:
    return ret;
}
EXPORT_SYMBOL(whitebox_ring_buffer_alloc);

void whitebox_ring_buffer_free(struct whitebox_ring_buffer* rb) {
    int i;
    d_printk(2, "\n");
    for (i = 0; i < rb->num_pages; ++i) {
        free_page(rb->page_mappings[i].page);
        rb->page_mappings[i].page = 0;
    }

    kfree(rb->page_mappings);
}
EXPORT_SYMBOL(whitebox_ring_buffer_free);

void whitebox_ring_buffer_init(struct whitebox_ring_buffer* rb) {
    int i;
    unsigned long flags;

    d_printk(2, "\n");
    spin_lock_irqsave(&rb->lock, flags);

    rb->write_page = 0;
    rb->writeable_pages = rb->num_pages;
    rb->read_page = 0;
    rb->readable_pages = 0;
    rb->dma_active = 0;

    // Recover the ringbuffer pages back to the kernel.
    for (i = 0; i < rb->num_pages; ++i) {
        rb->page_mappings[i].flags = RB_KERNEL;
    }
    spin_unlock_irqrestore(&rb->lock, flags);
}
EXPORT_SYMBOL(whitebox_ring_buffer_init);

int whitebox_ring_buffer_write_page_from_user(struct whitebox_ring_buffer* rb,
                                              const char __user* buf,
                                              size_t count) {
    int ret = 0;
    unsigned long flags;
    int write_page = -EINVAL;
    int writeable_pages;
    int readable_pages;
    struct ring_buffer_page_mapping* mapping;

    if (count >= PAGE_SIZE) {
        ret = -ENOMEM;
        goto done;
    }

    spin_lock_irqsave(&rb->lock, flags);
    write_page = rb->write_page;
    writeable_pages = rb->writeable_pages;

    if (!writeable_pages) {
        spin_unlock_irqrestore(&rb->lock, flags);
        ret = -ENOMEM;
        goto fail_overrun;
    }

    /*
     * Make sure that the write page is available for writing
     */
    mapping = &rb->page_mappings[write_page];
    if (!(mapping->flags & RB_KERNEL)) {
        //d_printk_rb(0, rb);
        spin_unlock_irqrestore(&rb->lock, flags);
        //d_printk(1, "overrun %x\n", mapping->flags);
        d_printk_rb(1, rb);
        ret = -ENOMEM;
        goto fail_overrun;
    }

    /*
     * Update page record keeping
     */
    mapping->flags = RB_USER;
    mapping->cnt = count;
    rb->write_page++;
    if (rb->write_page >= rb->num_pages) {
        rb->write_page = 0;
    }

    readable_pages = ++rb->readable_pages;
    writeable_pages = --rb->writeable_pages;

    spin_unlock_irqrestore(&rb->lock, flags);

    /*
     * Now, do the copy from the user address
     */
    if (copy_from_user((void*)mapping->page, buf, count) != 0) {
        d_printk(0, "failed to copy data from user\n");
        ret = -EFAULT;
        goto fail_copy_from_user;
    }

    d_printk(3, "wrote to page %d, writeable=%d readable=%d\n", write_page, writeable_pages, readable_pages);

    ret = count;

    goto done;

fail_copy_from_user:
    spin_lock_irqsave(&rb->lock, flags);
    --rb->readable_pages;
    ++rb->writeable_pages;
    rb->write_page--;
    if (rb->write_page < 0) {
        rb->write_page = rb->num_pages - 1;
    }
    mapping->flags = RB_KERNEL;
    spin_unlock_irqrestore(&rb->lock, flags);
fail_overrun:
done:
    return ret;
}

int whitebox_ring_buffer_write_from_user(struct whitebox_ring_buffer* rb,
                                         const char __user* buf,
                                         size_t count) {
    int written_count = 0;
    while (count > 0) {
        if(whitebox_ring_buffer_writeable_pages(rb) > 0) {
            int this_count;
            int this_ret;
            if (count >= PAGE_SIZE)
                this_count = PAGE_SIZE - 1;
            else
                this_count = count;
            this_ret = whitebox_ring_buffer_write_page_from_user(rb, buf, this_count);
            if (this_ret < 0)
                return this_ret;
            count -= this_count;
        } else {
            return written_count;
        }
    }
    return written_count;
}
EXPORT_SYMBOL(whitebox_ring_buffer_write_from_user);

int whitebox_ring_buffer_read_dma_start(struct whitebox_ring_buffer* rb,
                                        dma_addr_t* dma_mapping) {
    unsigned long flags;
    int read_page = -EINVAL;
    int readable_pages;
    int writeable_pages;
    int dma_active;
    struct ring_buffer_page_mapping* mapping;
    
    spin_lock_irqsave(&rb->lock, flags);
    read_page = rb->read_page;
    readable_pages = rb->readable_pages;
    dma_active = rb->dma_active;

    mapping = &rb->page_mappings[read_page];

    /*if (dma_active == 2) {
        spin_unlock_irqrestore(&rb->lock, flags);
        return 0;
    }
    else */if (readable_pages == 0) {
        spin_unlock_irqrestore(&rb->lock, flags);
        return 0;
    }
    else if (mapping->flags & RB_DMA) {
        spin_unlock_irqrestore(&rb->lock, flags);
        //d_printk(0, "****** WEIRD2 ****\n");
        return 0;
    }
    else if (!(mapping->flags & RB_USER)) {
        spin_unlock_irqrestore(&rb->lock, flags);
        // TODO: better error
        d_printk(0, "****** WEIRD ****\n");
        d_printk_rb(0, rb);
        d_printk(3, "** W ** reading from page %d, readable=%d\n", read_page, readable_pages);
        return -EINVAL;
    }

    // TODO again, is this the page a physical address !?
    mapping->dma_mapping = dma_map_single(NULL,
            (void*)mapping->page, mapping->cnt, DMA_TO_DEVICE);
    if (dma_mapping_error(NULL, mapping->dma_mapping)) {
        spin_unlock_irqrestore(&rb->lock, flags);
        d_printk(0, "failed to map dma\n");
        return -EFAULT;
    }

    readable_pages = --rb->readable_pages;
    writeable_pages = rb->writeable_pages;
    ++rb->dma_active;

    *dma_mapping = mapping->dma_mapping;
    mapping->flags = RB_DMA;

    spin_unlock_irqrestore(&rb->lock, flags);

    d_printk(3, "reading from page %d, readable=%d, writeable=%d\n", read_page, readable_pages, writeable_pages);

    return mapping->cnt;
}
EXPORT_SYMBOL(whitebox_ring_buffer_read_dma_start);

void whitebox_ring_buffer_read_dma_finish(struct whitebox_ring_buffer* rb) {
    unsigned long flags;
    int read_page = -EINVAL;
    int readable_pages;
    int writeable_pages;
    struct ring_buffer_page_mapping* mapping;

    spin_lock_irqsave(&rb->lock, flags);

    read_page = rb->read_page;
    mapping = &rb->page_mappings[read_page];

    if (!(mapping->flags & RB_DMA)) {
        spin_unlock_irqrestore(&rb->lock, flags);
        d_printk(1, "warning current page was not in dma");
        return;
    }

    /*
     * Return the page to the kernel
     */
    mapping->flags = RB_KERNEL;

    /*
     * Move to the next page for reading
     */
    rb->read_page++;
    if (rb->read_page >= rb->num_pages) {
        rb->read_page = 0;
    }
    readable_pages = rb->readable_pages;
    writeable_pages = ++rb->writeable_pages;
    --rb->dma_active;

    d_printk_rb(4, rb);

    spin_unlock_irqrestore(&rb->lock, flags);

    d_printk(3, "finished reading from page %d, readable=%d writeable=%d\n", read_page, readable_pages, writeable_pages);

    /*
     * Unmap the dma transfer
     */
    dma_unmap_single(NULL, mapping->dma_mapping, mapping->cnt, DMA_TO_DEVICE);

}
EXPORT_SYMBOL(whitebox_ring_buffer_read_dma_finish);

int whitebox_ring_buffer_writeable_pages(struct whitebox_ring_buffer* rb) {
    int ret;
    unsigned long flags;
    spin_lock_irqsave(&rb->lock, flags);
    ret = rb->writeable_pages;
    spin_unlock_irqrestore(&rb->lock, flags);
    return ret;
}
EXPORT_SYMBOL(whitebox_ring_buffer_writeable_pages);
