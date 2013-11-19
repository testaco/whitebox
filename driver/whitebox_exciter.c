#include <asm/io.h>
#include <linux/vmalloc.h>

#include "pdma.h"
#include "whitebox.h"

static int whitebox_rf_exciter_debug = 0;
#define d_printk(level, fmt, args...)				\
	if (whitebox_rf_exciter_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void _exciter_free(struct whitebox_exciter *exciter)
{
    iounmap(exciter->regs);
}

u32 _exciter_get_state(struct whitebox_exciter *exciter)
{
    return WHITEBOX_EXCITER(exciter)->state;
}

void _exciter_set_state(struct whitebox_exciter *exciter, u32 state_mask)
{
    WHITEBOX_EXCITER(exciter)->state |= state_mask;
}

void _exciter_clear_state(struct whitebox_exciter *exciter, u32 state_mask)
{
    WHITEBOX_EXCITER(exciter)->state &= ~state_mask;
}

u32 _exciter_get_interp(struct whitebox_exciter *exciter)
{
    return WHITEBOX_EXCITER(exciter)->interp;
}

void _exciter_set_interp(struct whitebox_exciter *exciter, u32 interp)
{
    WHITEBOX_EXCITER(exciter)->interp = interp;
}

u32 _exciter_get_fcw(struct whitebox_exciter *exciter)
{
    return WHITEBOX_EXCITER(exciter)->fcw;
}

void _exciter_set_fcw(struct whitebox_exciter *exciter, u32 fcw)
{
    WHITEBOX_EXCITER(exciter)->fcw = fcw;
}

u32 _exciter_get_threshold(struct whitebox_exciter *exciter)
{
    return WHITEBOX_EXCITER(exciter)->threshold;
}

void _exciter_set_threshold(struct whitebox_exciter *exciter, u32 threshold)
{
    WHITEBOX_EXCITER(exciter)->threshold = threshold;
}

void _exciter_get_runs(struct whitebox_exciter *exciter,
        u16 *overruns, u16 *underruns)
{
    u32 runs;
    runs = WHITEBOX_EXCITER(exciter)->runs;
    *overruns = (u16)((runs & WER_OVERRUNS_MASK) >> WER_OVERRUNS_OFFSET);
    *underruns = (u16)(runs & WER_UNDERRUNS_MASK);
}

long _exciter_space_available(struct whitebox_exciter *exciter,
        unsigned long *dest)
{
    u32 state;
    state = exciter->ops->get_state(exciter);
    *dest = (unsigned long)&WHITEBOX_EXCITER(exciter)->sample;
    if (!(state & WES_AFULL))
        return exciter->quantum;
    if (state & WES_SPACE)
        return 1;
    return 0;
}

int _exciter_produce(struct whitebox_exciter *exciter,
        size_t count)
{
    return 0;
}

struct whitebox_exciter_operations _exciter_ops = {
    .free = _exciter_free,
    .get_state = _exciter_get_state,
    .set_state = _exciter_set_state,
    .clear_state = _exciter_clear_state,
    .get_interp = _exciter_get_interp,
    .set_interp = _exciter_set_interp,
    .get_fcw = _exciter_get_fcw,
    .set_fcw = _exciter_set_fcw,
    .get_threshold = _exciter_get_threshold,
    .set_threshold = _exciter_set_threshold,
    .get_runs = _exciter_get_runs,
    .space_available = _exciter_space_available,
    .produce = _exciter_produce,
};

int whitebox_exciter_create(struct whitebox_exciter *exciter,
        unsigned long regs_start, size_t regs_size)
{
    exciter->ops = &_exciter_ops;
    exciter->regs = ioremap(regs_start, regs_size);
    if (!exciter->regs) {
        d_printk(0, "unable to map registers for "
            "whitebox exciter base=%08lx\n", regs_start);
        return -EINVAL;
    }
    exciter->pdma_config =
            PDMA_CONTROL_PER_SEL_FPGA0 |
            //PDMA_CONTROL_HIGH_PRIORITY |
            PDMA_CONTROL_XFER_SIZE_4B |
            PDMA_CONTROL_DST_ADDR_INC_0 |
            PDMA_CONTROL_SRC_ADDR_INC_4 |
            PDMA_CONTROL_PERIPH |
            PDMA_CONTROL_DIR_MEM_TO_PERIPH |
            PDMA_CONTROL_INTEN;
    return 0;
}

void _mock_exciter_free(struct whitebox_exciter *exciter)
{
    vfree(exciter->regs);
}

u32 _mock_exciter_get_state(struct whitebox_exciter *exciter)
{
    struct whitebox_mock_exciter *mock_exciter = 
        container_of(exciter, struct whitebox_mock_exciter, exciter);
    u32 state;
    long tail, head, space;
    long tail2, head2, data;

    state = WHITEBOX_EXCITER(exciter)->state;

    head = mock_exciter->buf->head;
    tail = ACCESS_ONCE(mock_exciter->buf->tail);
    space = CIRC_SPACE(head, tail, mock_exciter->buf_size);
    if (space)
        state |= WES_SPACE;
    if (space < exciter->quantum)
        state |= WES_AFULL;

    head2 = mock_exciter->buf->head;
    tail2 = ACCESS_ONCE(mock_exciter->buf->tail);
    data = CIRC_CNT(head2, tail2, mock_exciter->buf_size);
    if (data)
        state |= WES_DATA;
    if (data < exciter->quantum)
        state |= WES_AEMPTY;

    d_printk(1, "space=%ld data=%ld\n", space, data);

    return state;
}

void _mock_exciter_set_state(struct whitebox_exciter *exciter, u32 state_mask)
{
    struct whitebox_mock_exciter *mock_exciter = 
        container_of(exciter, struct whitebox_mock_exciter, exciter);
    u32 state;
    if (state_mask & WES_CLEAR) {
        WHITEBOX_EXCITER(exciter)->state = 0;
        mock_exciter->buf->head = 0;
        mock_exciter->buf->tail = 0;
        return;
    }
    if (state_mask & WES_TXSTOP) {
        exciter->ops->clear_state(exciter, WES_TXEN);
        return;
    }
    state = WHITEBOX_EXCITER(exciter)->state;
    WHITEBOX_EXCITER(exciter)->state = state | state_mask;
}

void _mock_exciter_clear_state(struct whitebox_exciter *exciter, u32 state_mask)
{
    u32 state;
    d_printk(1, "\n");
    state = WHITEBOX_EXCITER(exciter)->state;
    WHITEBOX_EXCITER(exciter)->state = state & ~state_mask;
}

long _mock_exciter_space_available(struct whitebox_exciter *exciter,
        unsigned long *dest)
{
    struct whitebox_mock_exciter *mock_exciter = 
        container_of(exciter, struct whitebox_mock_exciter, exciter);
    u32 state = exciter->ops->get_state(exciter);
    d_printk(1, "\n");
    *dest = (unsigned long)mock_exciter->buf->buf + mock_exciter->buf->head;
    if (!(state & WES_AFULL))
        return exciter->quantum;
    if (state & WES_SPACE)
        return 4;
    return 0;
}

int _mock_exciter_produce(struct whitebox_exciter *exciter,
        size_t count)
{
    struct whitebox_mock_exciter *mock_exciter = 
        container_of(exciter, struct whitebox_mock_exciter, exciter);
    d_printk(1, "values... %08x %08x %08x %08x\n",
            (u32)*(mock_exciter->buf->buf + mock_exciter->buf->head + 0),
            (u32)*(mock_exciter->buf->buf + mock_exciter->buf->head + 4),
            (u32)*(mock_exciter->buf->buf + mock_exciter->buf->head + 8),
            (u32)*(mock_exciter->buf->buf + mock_exciter->buf->head + 12));
    mock_exciter->buf->head = (mock_exciter->buf->head + count) &
        (mock_exciter->buf_size - 1);
    return 0;
}

struct whitebox_exciter_operations _mock_exciter_ops = {
    .free = _mock_exciter_free,
    .get_state = _mock_exciter_get_state,
    .set_state = _mock_exciter_set_state,
    .clear_state = _mock_exciter_clear_state,
    .get_interp = _exciter_get_interp,
    .set_interp = _exciter_set_interp,
    .get_fcw = _exciter_get_fcw,
    .set_fcw = _exciter_set_fcw,
    .get_threshold = _exciter_get_threshold,
    .set_threshold = _exciter_set_threshold,
    .get_runs = _exciter_get_runs,
    .space_available = _mock_exciter_space_available,
    .produce = _mock_exciter_produce,
};

int whitebox_mock_exciter_create(struct whitebox_mock_exciter *mock_exciter,
        size_t regs_size, int order, struct circ_buf *buf)
{
    struct whitebox_exciter *exciter = &mock_exciter->exciter;
    exciter->ops = &_mock_exciter_ops;
    exciter->regs = vmalloc(regs_size);
    if (!exciter->regs) {
        d_printk(0, "unable to alloc registers for "
            "whitebox mock exciter\n");
        return -EINVAL;
    }
    exciter->pdma_config =
            //PDMA_CONTROL_HIGH_PRIORITY |
            PDMA_CONTROL_XFER_SIZE_4B |
            PDMA_CONTROL_DST_ADDR_INC_4 |
            PDMA_CONTROL_SRC_ADDR_INC_4 |
            PDMA_CONTROL_DIR_MEM_TO_PERIPH |
            PDMA_CONTROL_INTEN;
    mock_exciter->order = order;
    mock_exciter->buf_size = PAGE_SIZE << mock_exciter->order;
    mock_exciter->buf = buf;
    mock_exciter->buf->head = 0;
    mock_exciter->buf->tail = 0;
    return 0;
}

void whitebox_exciter_destroy(struct whitebox_exciter *exciter)
{
    exciter->ops->free(exciter);
}
