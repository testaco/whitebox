#include <asm/io.h>
#include <linux/vmalloc.h>

#include "pdma.h"
#include "whitebox.h"

#define d_printk(level, fmt, args...)				\
	if (whitebox_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)


void _receiver_free(struct whitebox_receiver *receiver)
{
    iounmap(receiver->regs);
}

u32 _receiver_get_state(struct whitebox_receiver *receiver)
{
    return WHITEBOX_RECEIVER(receiver)->state;
}

void _receiver_set_state(struct whitebox_receiver *receiver, u32 state_mask)
{
    WHITEBOX_RECEIVER(receiver)->state |= state_mask;
}

void _receiver_clear_state(struct whitebox_receiver *receiver, u32 state_mask)
{
    WHITEBOX_RECEIVER(receiver)->state &= ~state_mask;
}

u32 _receiver_get_decim(struct whitebox_receiver *receiver)
{
    return WHITEBOX_RECEIVER(receiver)->decim;
}

void _receiver_set_decim(struct whitebox_receiver *receiver, u32 decim)
{
    WHITEBOX_RECEIVER(receiver)->decim = decim;
}

u32 _receiver_get_fcw(struct whitebox_receiver *receiver)
{
    return WHITEBOX_RECEIVER(receiver)->fcw;
}

void _receiver_set_fcw(struct whitebox_receiver *receiver, u32 fcw)
{
    WHITEBOX_RECEIVER(receiver)->fcw = fcw;
}

u32 _receiver_get_threshold(struct whitebox_receiver *receiver)
{
    return WHITEBOX_RECEIVER(receiver)->threshold;
}

void _receiver_set_threshold(struct whitebox_receiver *receiver, u32 threshold)
{
    WHITEBOX_RECEIVER(receiver)->threshold = threshold;
}

u32 _receiver_get_correction(struct whitebox_receiver *receiver)
{
    return WHITEBOX_RECEIVER(receiver)->correction;
}

void _receiver_set_correction(struct whitebox_receiver *receiver, u32 correction)
{
    WHITEBOX_RECEIVER(receiver)->correction = correction;
}


void _receiver_get_runs(struct whitebox_receiver *receiver,
        u16 *overruns, u16 *underruns)
{
    u32 runs;
    runs = WHITEBOX_RECEIVER(receiver)->runs;
    *overruns = (u16)((runs & WRR_OVERRUNS_MASK) >> WRR_OVERRUNS_OFFSET);
    *underruns = (u16)(runs & WRR_UNDERRUNS_MASK);
}

long _receiver_data_available(struct whitebox_receiver *receiver,
        unsigned long *src)
{
    *src = (unsigned long)&WHITEBOX_RECEIVER(receiver)->sample;
    return whitebox_frame_size << 2;
}

int _receiver_consume(struct whitebox_receiver *receiver,
        size_t count)
{
    return 0;
}

u32 _receiver_get_debug(struct whitebox_receiver *receiver)
{
    return WHITEBOX_RECEIVER(receiver)->debug;
}


struct whitebox_receiver_operations _receiver_ops = {
    .free = _receiver_free,
    .get_state = _receiver_get_state,
    .set_state = _receiver_set_state,
    .clear_state = _receiver_clear_state,
    .get_decim = _receiver_get_decim,
    .set_decim = _receiver_set_decim,
    .get_fcw = _receiver_get_fcw,
    .set_fcw = _receiver_set_fcw,
    .get_threshold = _receiver_get_threshold,
    .set_threshold = _receiver_set_threshold,
    .get_correction = _receiver_get_correction,
    .set_correction = _receiver_set_correction,
    .get_runs = _receiver_get_runs,
    .data_available = _receiver_data_available,
    .consume = _receiver_consume,
    .get_debug = _receiver_get_debug,
};

int whitebox_receiver_create(struct whitebox_receiver *receiver,
        unsigned long regs_start, size_t regs_size)
{
    receiver->ops = &_receiver_ops;
    receiver->incr_src = 0;
    receiver->regs = ioremap(regs_start, regs_size);
    if (!receiver->regs) {
        d_printk(0, "unable to map registers for "
            "whitebox receiver base=%08lx\n", regs_start);
        return -EINVAL;
    }
    receiver->pdma_config =
            PDMA_CONTROL_PER_SEL_FPGA0 |
            //PDMA_CONTROL_HIGH_PRIORITY |
            PDMA_CONTROL_XFER_SIZE_4B |
            PDMA_CONTROL_DST_ADDR_INC_4 |
            PDMA_CONTROL_SRC_ADDR_INC_0 |
            PDMA_CONTROL_PERIPH |
            PDMA_CONTROL_DIR_PERIPH_TO_MEM |
            PDMA_CONTROL_INTEN;
    return 0;
}

void _mock_receiver_free(struct whitebox_receiver *receiver)
{
    vfree(receiver->regs);
}

u32 _mock_receiver_get_state(struct whitebox_receiver *receiver)
{
    struct whitebox_mock_receiver *mock_receiver = 
        container_of(receiver, struct whitebox_mock_receiver, receiver);
    u32 state;
    long tail, head, space;
    long tail2, head2, data;

    state = WHITEBOX_RECEIVER(receiver)->state;

    head = mock_receiver->buf->head;
    tail = ACCESS_ONCE(mock_receiver->buf->tail);
    space = CIRC_SPACE(head, tail, mock_receiver->buf_size);
    if (space >> 2)
        state |= WRS_SPACE;
    if (space < receiver->quantum)
        state |= WRS_AFULL;

    head2 = mock_receiver->buf->head;
    tail2 = ACCESS_ONCE(mock_receiver->buf->tail);
    data = CIRC_CNT(head2, tail2, mock_receiver->buf_size);
    if (data >> 2)
        state |= WRS_DATA;
    if (data < receiver->quantum)
        state |= WRS_AEMPTY;

    d_printk(1, "space=%ld data=%ld\n", space, data);

    return state;
}

void _mock_receiver_set_state(struct whitebox_receiver *receiver, u32 state_mask)
{
    struct whitebox_mock_receiver *mock_receiver = 
        container_of(receiver, struct whitebox_mock_receiver, receiver);
    u32 state;
    if (state_mask & WS_CLEAR) {
        WHITEBOX_RECEIVER(receiver)->state = 0;
        mock_receiver->buf->head = 0;
        mock_receiver->buf->tail = 0;
        return;
    }
    if (state_mask & WRS_RXSTOP) {
        receiver->ops->clear_state(receiver, WRS_RXEN);
        return;
    }
    state = WHITEBOX_RECEIVER(receiver)->state;
    WHITEBOX_RECEIVER(receiver)->state = state | state_mask;
}

void _mock_receiver_clear_state(struct whitebox_receiver *receiver, u32 state_mask)
{
    u32 state;
    d_printk(1, "\n");
    state = WHITEBOX_RECEIVER(receiver)->state;
    WHITEBOX_RECEIVER(receiver)->state = state & ~state_mask;
}

long _mock_receiver_data_available(struct whitebox_receiver *receiver,
        unsigned long *src)
{
    struct whitebox_mock_receiver *mock_receiver = 
        container_of(receiver, struct whitebox_mock_receiver, receiver);
    long head, tail, data;
    d_printk(1, "\n");
    head = ACCESS_ONCE(mock_receiver->buf->head);
    tail = mock_receiver->buf->tail;
    data = CIRC_CNT_TO_END(head, tail, mock_receiver->buf_size) & ~3;
    /*if (data < 4) {
        mock_receiver->buf->head = head = mock_receiver->buf->tail = tail = 0;
        data = CIRC_CNT_TO_END(head, tail, mock_receiver->buf_size);
    }*/
    *src = (unsigned long)mock_receiver->buf->buf + tail;
    return min(data, 1024L);
}

int _mock_receiver_consume(struct whitebox_receiver *receiver,
        size_t count)
{
    struct whitebox_mock_receiver *mock_receiver = 
        container_of(receiver, struct whitebox_mock_receiver, receiver);
    d_printk(1, "values... %08x %08x %08x %08x\n",
            (u32)*(mock_receiver->buf->buf + mock_receiver->buf->tail + 0),
            (u32)*(mock_receiver->buf->buf + mock_receiver->buf->tail + 4),
            (u32)*(mock_receiver->buf->buf + mock_receiver->buf->tail + 8),
            (u32)*(mock_receiver->buf->buf + mock_receiver->buf->tail + 12));
    mock_receiver->buf->tail = (mock_receiver->buf->tail + count) &
        (mock_receiver->buf_size - 4);
    return 0;
}

struct whitebox_receiver_operations _mock_receiver_ops = {
    .free = _mock_receiver_free,
    .get_state = _mock_receiver_get_state,
    .set_state = _mock_receiver_set_state,
    .clear_state = _mock_receiver_clear_state,
    .get_decim = _receiver_get_decim,
    .set_decim = _receiver_set_decim,
    .get_fcw = _receiver_get_fcw,
    .set_fcw = _receiver_set_fcw,
    .get_threshold = _receiver_get_threshold,
    .set_threshold = _receiver_set_threshold,
    .get_correction = _receiver_get_correction,
    .set_correction = _receiver_set_correction,
    .get_runs = _receiver_get_runs,
    .data_available = _mock_receiver_data_available,
    .consume = _mock_receiver_consume,
    .get_debug = _receiver_get_debug,
};

int whitebox_mock_receiver_create(struct whitebox_mock_receiver *mock_receiver,
        size_t regs_size, int order, struct circ_buf *buf)
{
    struct whitebox_receiver *receiver = &mock_receiver->receiver;
    receiver->ops = &_mock_receiver_ops;
    receiver->incr_src = 1;
    receiver->regs = vmalloc(regs_size);
    if (!receiver->regs) {
        d_printk(0, "unable to alloc registers for "
            "whitebox mock receiver\n");
        return -EINVAL;
    }
    receiver->pdma_config =
            //PDMA_CONTROL_HIGH_PRIORITY |
            PDMA_CONTROL_XFER_SIZE_4B |
            PDMA_CONTROL_DST_ADDR_INC_4 |
            PDMA_CONTROL_SRC_ADDR_INC_4 |
            PDMA_CONTROL_DIR_MEM_TO_PERIPH |
            PDMA_CONTROL_INTEN;

    mock_receiver->order = order;
    mock_receiver->buf_size = PAGE_SIZE << mock_receiver->order;
    mock_receiver->buf = buf;
    mock_receiver->buf->head = 0;
    mock_receiver->buf->tail = 0;
    return 0;
}

void whitebox_receiver_destroy(struct whitebox_receiver *receiver)
{
    receiver->ops->free(receiver);
}
