#ifndef __WHITEBOX_RING_BUFFER_H
#define __WHITEBOX_RING_BUFFER_H

#include <linux/spinlock.h>

/*
 * A ring buffer is used to move samples from userspace to kernelspace and
 * vice versa.
 *
 * This code is based on the ring buffer implementation by Philip Ballister
 * for the Ettus USRP E100.
 */
typedef struct whitebox_ring_buffer {
    int num_pages;
    spinlock_t lock;
    int dma_active;

    int writeable_pages;
    int write_page;

    int readable_pages;
    int read_page;

    struct ring_buffer_page_mapping {
        unsigned long page;
        u8 flags;
        u16 cnt;
        dma_addr_t dma_mapping;
    } *page_mappings;
} whitebox_ring_buffer_t;

/*
 * Flags for the ring buffer
 */
#define RB_KERNEL   0x01  // Kernel can use
#define RB_USER     0x02  // Storing user data
#define RB_DMA      0x04  // In DMA transfer

int whitebox_ring_buffer_alloc(struct whitebox_ring_buffer* rb, unsigned num_pages);
void whitebox_ring_buffer_free(struct whitebox_ring_buffer* rb);
void whitebox_ring_buffer_init(struct whitebox_ring_buffer* rb);

// This is for TX
int whitebox_ring_buffer_write_from_user(struct whitebox_ring_buffer* rb,
                                         const char __user* buf,
                                         size_t count);
int whitebox_ring_buffer_read_dma_start(struct whitebox_ring_buffer* rb,
                                        dma_addr_t* mapping);
void whitebox_ring_buffer_read_dma_finish(struct whitebox_ring_buffer* rb);

int whitebox_ring_buffer_writeable_pages(struct whitebox_ring_buffer* rb);

#endif /* __WHITEBOX_RING_BUFFER_H */
