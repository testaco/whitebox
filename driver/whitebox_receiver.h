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
    u32 interp;
    u32 fcw;
    u32 runs;
    u32 threshold;
};

#define WHITEBOX_RECEIVER(e) ((volatile struct whitebox_receiver_regs *)((e)->regs))

struct whitebox_receiver_operations;

struct whitebox_receiver {
    void *regs;
    size_t quantum;
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

    u32 (*get_interp)(struct whitebox_receiver *receiver);
    void (*set_interp)(struct whitebox_receiver *receiver, u32 interp);

    u32 (*get_fcw)(struct whitebox_receiver *receiver);
    void (*set_fcw)(struct whitebox_receiver *receiver, u32 fcw);

    u32 (*get_threshold)(struct whitebox_receiver *receiver);
    void (*set_threshold)(struct whitebox_receiver *receiver, u32 threshold);

    void (*get_runs)(struct whitebox_receiver *receiver,
            u16 *overruns, u16 *underruns);

    long (*data_available)(struct whitebox_receiver *receiver,
            unsigned long *src);
    int (*consume)(struct whitebox_receiver *receiver,
            size_t count);
};

int whitebox_receiver_create(struct whitebox_receiver *receiver,
        unsigned long regs_start, size_t regs_size);

int whitebox_mock_receiver_create(struct whitebox_mock_receiver *receiver,
        size_t regs_size, int order, struct circ_buf *buf);

void whitebox_receiver_destroy(struct whitebox_receiver *receiver);

#endif /* __WHITEBOX_RECEIVER_H */
