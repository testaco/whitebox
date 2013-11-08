#ifndef __WHITEBOX_BLOCK_H
#define __WHITEBOX_BLOCK_H

#include <linux/circ_buf.h>

#if 0
struct whitebox_block_operations {
    /* Allocates any necessary data */
    void (*alloc)(struct whitebox_block *block);
    /* Callback to free any allocated data */
    void (*free)(struct whitebox_block *block);

    /* How much space is available, and at what address does it start */
    long (*space_available)(struct whitebox_block *block, long *dest);

    /* Produce data into the block */
    long (*produce)(struct whitebox_block *block, size_t count);

    /* How much data is available, and at what address does it start */
    long (*data_available)(struct whitebox_block *block, long *src);

    /* Consume data from the block */
    long (*consume)(struct whitebox_block *block, size_t count);

    int (*work)(struct whitebox_block *block, unsigned long src, size_t src_count, unsigned_long dest, size_t dest_count);
};

struct whitebox_block {
    struct whitebox_block_operations *b_ops;
    /* lock */
    spinlock_t b_lock;
    /* where the source is, absolute byte offset */
    loff_t b_off;
}
#endif

struct whitebox_user_source {
    /* where the source is, absolute byte offset */
    loff_t off;
    /* 2**order pages for buf size */
    int order;
    /* indicating copy_to_user vs mmap */
    atomic_t *mapped;
    /* the size of the buf */
    long buf_size;
    /* the buf */
    struct circ_buf buf;
};

void whitebox_user_source_init(struct whitebox_user_source *user_source,
        int order, atomic_t *mapped);
int whitebox_user_source_alloc(struct whitebox_user_source *user_source);
void whitebox_user_source_free(struct whitebox_user_source *user_source);
size_t whitebox_user_source_space_available(struct whitebox_user_source *user_source,
        unsigned long *dest);
int whitebox_user_source_produce(struct whitebox_user_source *user_source,
        size_t count);
size_t whitebox_user_source_data_available(struct whitebox_user_source *user_source,
        unsigned long *src);
int whitebox_user_source_consume(struct whitebox_user_source *user_source,
        size_t count);
int whitebox_user_source_work(struct whitebox_user_source *user_source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count);


struct whitebox_rf_sink;

struct whitebox_rf_sink_operations {
    u32 (*get_state)(struct whitebox_rf_sink *rf_sink);
    void (*set_state)(struct whitebox_rf_sink *rf_sink, u32 state_mask);
    void (*clear_state)(struct whitebox_rf_sink *rf_sink, u32 state_mask);
};

struct whitebox_rf_sink {
    /* locked at start of dma, must use trylock since its a tasklet */
    spinlock_t lock;
    /* where the source is, absolute byte offset */
    loff_t off;
    /* the dma channel */
    int dma_ch;
    /* the dma mapping */
    dma_addr_t dma_mapping;
    /* the dma mapped size */
    size_t dma_count;
    /* driver callback to keep flow going */
    void (*dma_cb)(void*);
    /* callback data */
    void *dma_cb_data;
    /* how many samples to send per dma callback */
    size_t quantum;

    /* exciter regs */
    unsigned long regs_start;
    size_t regs_size;
    void *regs;

    /* Operations to act on regs */
    struct whitebox_rf_sink_operations *rf_ops;
};

void whitebox_rf_sink_init(struct whitebox_rf_sink *rf_sink,
        unsigned long regs_start, size_t regs_size,
        int dma_ch, void (*dma_cb)(void*), void *dma_cb_data,
        size_t quantum);
int whitebox_rf_sink_alloc(struct whitebox_rf_sink *rf_sink);
void whitebox_rf_sink_free(struct whitebox_rf_sink *rf_sink);
size_t whitebox_rf_sink_space_available(struct whitebox_rf_sink *rf_sink,
        unsigned long *dest);
int whitebox_rf_sink_produce(struct whitebox_rf_sink *rf_sink, size_t count);
int whitebox_rf_sink_work(struct whitebox_rf_sink *rf_sink,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count);
int whitebox_rf_sink_work_done(struct whitebox_rf_sink *rf_sink);

#endif /* __WHITEBOX_BLOCK_H */
