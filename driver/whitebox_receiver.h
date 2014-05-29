#ifndef __WHITEBOX_RECEIVER_H
#define __WHITEBOX_RECEIVER_H

#include <linux/types.h>
#include <linux/circ_buf.h>

/*
 * IO Mapped structure of the receiver
 */
struct whitebox_receiver_regs {
    u32 sample;
    u32 state;
    u32 decim;
    u32 fcw;
    u32 runs;
    u32 threshold;
    u32 correction;
    u32 available;
    u32 debug;
};

#define WHITEBOX_RECEIVER(e) ((volatile struct whitebox_receiver_regs *)((e)->regs))

struct whitebox_receiver_operations;

struct whitebox_receiver {
    void *regs;
    size_t quantum;
    int incr_src;
    int dma_enable;
    int copy_enable;
    u32 pdma_config;
    struct whitebox_receiver_operations *ops;
};

struct whitebox_mock_receiver {
    struct whitebox_receiver receiver;
    unsigned long buf_size;
    int order;
    struct circ_buf *buf;
};

struct whitebox_receiver_operations {
    void (*free)(struct whitebox_receiver *receiver);
    u32 (*get_state)(struct whitebox_receiver *receiver);
    void (*set_state)(struct whitebox_receiver *receiver, u32 state_mask);
    void (*clear_state)(struct whitebox_receiver *receiver, u32 state_mask);

    u32 (*get_decim)(struct whitebox_receiver *receiver);
    void (*set_decim)(struct whitebox_receiver *receiver, u32 decim);

    u32 (*get_fcw)(struct whitebox_receiver *receiver);
    void (*set_fcw)(struct whitebox_receiver *receiver, u32 fcw);

    u32 (*get_threshold)(struct whitebox_receiver *receiver);
    void (*set_threshold)(struct whitebox_receiver *receiver, u32 threshold);

    u32 (*get_correction)(struct whitebox_receiver *receiver);
    void (*set_correction)(struct whitebox_receiver *receiver, u32 correction);

    void (*get_runs)(struct whitebox_receiver *receiver,
            u16 *overruns, u16 *underruns);

    long (*data_available)(struct whitebox_receiver *receiver,
            unsigned long *src);
    int (*consume)(struct whitebox_receiver *receiver,
            size_t count);

    u32 (*get_debug)(struct whitebox_receiver *receiver);
};

int whitebox_receiver_create(struct whitebox_receiver *receiver,
        unsigned long regs_start, size_t regs_size);

int whitebox_mock_receiver_create(struct whitebox_mock_receiver *receiver,
        size_t regs_size, int order, struct circ_buf *buf);

void whitebox_receiver_destroy(struct whitebox_receiver *receiver);

#endif /* __WHITEBOX_RECEIVER_H */
