#include <linux/uaccess.h>

#include "whitebox.h"
#include "whitebox_block.h"

#define d_printk(level, fmt, args...)				\
	if (whitebox_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void whitebox_user_sink_init(struct whitebox_user_sink *user_sink,
        int order, atomic_t *mapped)
{
    user_sink->order = order;
    user_sink->mapped = mapped;
}

int whitebox_user_sink_alloc(struct whitebox_user_sink *user_sink, unsigned long buf_addr)
{
    // alloc circ buffer
    user_sink->buf_size = PAGE_SIZE << user_sink->order;
    user_sink->buf.buf = (char*)buf_addr;
    if (!user_sink->buf.buf) {
        d_printk(0, "failed to create port buffer\n");
        return -ENOMEM;
    }
    d_printk(4, "%08lx\n", (unsigned long)user_sink->buf.buf);
    user_sink->buf.head = 0;
    user_sink->buf.tail = 0;
    return 0;
}

void whitebox_user_sink_free(struct whitebox_user_sink *user_sink)
{
    // release the circ buffer
    //free_pages((unsigned long)user_sink->buf.buf, user_sink->order);
}

size_t whitebox_user_sink_space_available(struct whitebox_user_sink *user_sink,
        unsigned long *dest)
{
    long tail, head, space;

    if (whitebox_flow_control) {
        if (whitebox_user_sink_data_total(user_sink) >= whitebox_user_sink_buffer_threshold)
            return 0;
    }

    head = user_sink->buf.head;
    tail = ACCESS_ONCE(user_sink->buf.tail);
    space = CIRC_SPACE_TO_END(head, tail, user_sink->buf_size);
    *dest = (unsigned long)user_sink->buf.buf + head;
    d_printk(7, "%ld\n", space);
    return space;
}

int whitebox_user_sink_produce(struct whitebox_user_sink *user_sink,
        size_t count)
{
    d_printk(3, "%zd values... %08x %08x %08x %08x\n",
            count,
            (u32)*(user_sink->buf.buf + user_sink->buf.head + 0),
            (u32)*(user_sink->buf.buf + user_sink->buf.head + 4),
            (u32)*(user_sink->buf.buf + user_sink->buf.head + 8),
            (u32)*(user_sink->buf.buf + user_sink->buf.head + 12));
    user_sink->buf.head = (user_sink->buf.head + count) & (user_sink->buf_size - 4);
    user_sink->off += count;
    return 0;
}

size_t whitebox_user_sink_data_available(struct whitebox_user_sink *user_sink,
        unsigned long *src)
{
    long head, tail, data;
    head = ACCESS_ONCE(user_sink->buf.head);
    tail = user_sink->buf.tail;
    data = CIRC_CNT_TO_END(head, tail, user_sink->buf_size);
    *src = (long)user_sink->buf.buf + tail;
    return data;
}

size_t whitebox_user_sink_data_total(struct whitebox_user_sink *user_sink)
{
    long head, tail, data;
    head = ACCESS_ONCE(user_sink->buf.head);
    tail = user_sink->buf.tail;
    data = CIRC_CNT(head, tail, user_sink->buf_size);
    d_printk(7, "%ld\n", data);
    return data;
}

int whitebox_user_sink_consume(struct whitebox_user_sink *user_sink,
        size_t count)
{
    d_printk(3, "%zd values... %08x %08x %08x %08x\n",
            count,
            (u32)*(user_sink->buf.buf + user_sink->buf.tail + 0),
            (u32)*(user_sink->buf.buf + user_sink->buf.tail + 4),
            (u32)*(user_sink->buf.buf + user_sink->buf.tail + 8),
            (u32)*(user_sink->buf.buf + user_sink->buf.tail + 12));
    user_sink->buf.tail = (user_sink->buf.tail + count) & (user_sink->buf_size - 1);
    return 0;
}

int whitebox_user_sink_work(struct whitebox_user_sink *user_sink,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    size_t count = min(src_count, dest_count);
    d_printk(2, "src=%08lx src_count=%zu dest=%08lx dest_count=%zu\n",
            src, src_count, dest, dest_count);

    if (!atomic_read(user_sink->mapped)) {
        if (copy_to_user((void*)dest, (void*)src, count) != 0) {
            d_printk(0, "failed to copy data to user\n");
            return -EFAULT;
        }
    }
    return (int)count;
}
