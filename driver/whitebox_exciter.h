#ifndef __WHITEBOX_EXCITER_H
#define __WHITEBOX_EXCITER_H

#include <linux/types.h>
#include <linux/circ_buf.h>

/*
 * IO Mapped structure of the exciter
 */
struct whitebox_exciter_regs {
    u32 sample;
    u32 state;
    u32 interp;
    u32 fcw;
    u32 runs;
    u32 threshold;
    u32 correction;
    u32 available;
    u32 debug;
    u32 gain;
    u32 fir;
};

#define WHITEBOX_EXCITER(e) ((volatile struct whitebox_exciter_regs *)((e)->regs))

struct whitebox_exciter_operations;

struct whitebox_exciter {
    void *regs;
    size_t quantum;
    int incr_dest;
    int auto_tx;
    int dma_enable;
    int copy_enable;
    u32 pdma_config;
    struct whitebox_exciter_operations *ops;
};

struct whitebox_mock_exciter {
    struct whitebox_exciter exciter;
    unsigned long buf_size;
    int order;
    struct circ_buf *buf;
    s32 fir_coeff[WF_COEFF_COUNT];
};

struct whitebox_exciter_operations {
    void (*free)(struct whitebox_exciter *exciter);
    u32 (*get_state)(struct whitebox_exciter *exciter);
    void (*set_state)(struct whitebox_exciter *exciter, u32 state_mask);
    void (*clear_state)(struct whitebox_exciter *exciter, u32 state_mask);

    u32 (*get_interp)(struct whitebox_exciter *exciter);
    void (*set_interp)(struct whitebox_exciter *exciter, u32 interp);

    u32 (*get_fcw)(struct whitebox_exciter *exciter);
    void (*set_fcw)(struct whitebox_exciter *exciter, u32 fcw);

    u32 (*get_threshold)(struct whitebox_exciter *exciter);
    void (*set_threshold)(struct whitebox_exciter *exciter, u32 threshold);

    u32 (*get_correction)(struct whitebox_exciter *exciter);
    void (*set_correction)(struct whitebox_exciter *exciter, u32 correction);

    u32 (*get_gain)(struct whitebox_exciter *exciter);
    void (*set_gain)(struct whitebox_exciter *exciter, u32 gain);

    void (*get_runs)(struct whitebox_exciter *exciter,
            u16 *overruns, u16 *underruns);

    long (*space_available)(struct whitebox_exciter *exciter,
            unsigned long *dest);
    int (*produce)(struct whitebox_exciter *exciter,
            size_t count);

    u32 (*get_debug)(struct whitebox_exciter *exctier);

    u32 (*get_fir)(struct whitebox_exciter *exciter);
    void (*set_fir)(struct whitebox_exciter *exciter, u32 fir);

    s32 (*get_fir_coeff)(struct whitebox_exciter *exciter, u8 i);
    void (*set_fir_coeff)(struct whitebox_exciter *exciter, u8 i, s32 c);
};

int whitebox_exciter_create(struct whitebox_exciter *exciter,
        unsigned long regs_start, size_t regs_size);

int whitebox_mock_exciter_create(struct whitebox_mock_exciter *exciter,
        size_t regs_size, int order, struct circ_buf *buf);

void whitebox_exciter_destroy(struct whitebox_exciter *exciter);

#endif /* __WHITEBOX_EXCITER_H */
