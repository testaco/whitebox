#include <linux/sched.h>
#include "whitebox.h"
#include "whitebox_gpio.h"

int rx_start(struct whitebox_device *wb)
{
    struct whitebox_receiver *receiver = wb->rf_source.receiver;
    receiver->ops->get_runs(receiver, &wb->cur_overruns, &wb->cur_underruns);
    return 0;
}

int rx_exec(struct whitebox_device* wb)
{
    struct whitebox_rf_source *rf_source = &wb->rf_source;
    struct whitebox_user_sink *user_sink = &wb->user_sink;
    size_t src_count, dest_count;
    unsigned long src, dest;
    size_t count;
    int result;

    // stats->calls++;

    if (!spin_trylock(&rf_source->lock))
        // stats->busy++;
        return -EBUSY;

    src_count = whitebox_rf_source_data_available(rf_source, &src);
    dest_count = whitebox_user_sink_space_available(user_sink, &dest);
    count = min(src_count, dest_count);
    
    if (count == 0) {
        // stats->blocked++;
        spin_unlock(&rf_source->lock);
        return -EBUSY;
    }

    result = whitebox_rf_source_work(rf_source, src, src_count, dest, dest_count);

    if (result < 0) {
        // stats->failed_work++;
        spin_unlock(&rf_source->lock);
        return result;
    }

    // stats->work++;

    // NOTE: Do not unlock the rf_source's lock - the dma_cb will do it

    return result;
}

void rx_dma_cb(void *data)
{
    struct whitebox_device *wb = (struct whitebox_device *)data;
    struct whitebox_rf_source *rf_source = &wb->rf_source;
    struct whitebox_user_sink *user_sink = &wb->user_sink;
    size_t count;

    // stats->cb_calls++:

    count = whitebox_rf_source_work_done(rf_source);
    whitebox_rf_source_consume(rf_source, count);
    whitebox_user_sink_produce(user_sink, count);

    spin_unlock(&rf_source->lock);

    wake_up_interruptible(&wb->read_wait_queue);

    rx_exec(wb);
}

void rx_stop(struct whitebox_device *wb)
{
    // wait for DMA to finish
    /*while (pdma_active(whitebox_device->rx_dma_ch) > 0) {
        cpu_relax();
    }*/

    wb->rf_source.receiver->ops->set_state(wb->rf_source.receiver, WRS_RXSTOP);
}

int rx_error(struct whitebox_device *wb)
{
    if (whitebox_check_plls) {
        int c, locked;
        c = whitebox_gpio_cmx991_read(wb->platform_data,
            WHITEBOX_CMX991_LD_REG);
        locked = whitebox_gpio_adf4351_locked(wb->platform_data)
                && (c & WHITEBOX_CMX991_LD_MASK);
        if (!locked)
            return 1;
    }

    if (whitebox_check_runs) {
        u16 overruns, underruns;
        wb->rf_source.receiver->ops->get_runs(wb->rf_source.receiver,
                &overruns, &underruns);
        if (wb->cur_overruns != overruns) {
            wb->cur_overruns = overruns;
            return 2;
        }
        if (wb->cur_underruns != underruns) {
            wb->cur_underruns = underruns;
            return 3;
        }
    }
    return 0;
}

/*static irqreturn_t tx_irq_cb(int irq, void* ptr) {
    struct whitebox_device* wb = (struct whitebox_device*)ptr;
    rx_exec(wb);

    return IRQ_HANDLED;
}*/

