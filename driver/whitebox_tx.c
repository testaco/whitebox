#include <linux/sched.h>
#include "whitebox.h"
#include "whitebox_gpio.h"

int tx_start(struct whitebox_device *wb)
{
    struct whitebox_exciter *exciter = wb->rf_sink.exciter;
    exciter->ops->set_state(exciter, WES_CLEAR);
    exciter->ops->get_runs(exciter, &wb->cur_overruns, &wb->cur_underruns);
    return 0;
}

int tx_exec(struct whitebox_device* wb)
{
    struct whitebox_user_source *user_source = &wb->user_source;
    struct whitebox_rf_sink *rf_sink = &wb->rf_sink;
    size_t src_count, dest_count;
    unsigned long src, dest;
    size_t count;
    int result;

    // stats->calls++;

    if (!spin_trylock(&rf_sink->lock))
        // stats->busy++;
        return -EBUSY;

    src_count = whitebox_user_source_data_available(user_source, &src);
    dest_count = whitebox_rf_sink_space_available(rf_sink, &dest);
    count = min(src_count, dest_count);
    
    if (count == 0) {
        // stats->blocked++;
        spin_unlock(&rf_sink->lock);
        return -EBUSY;
    }

    result = whitebox_rf_sink_work(rf_sink, src, src_count, dest, dest_count);

    if (result < 0) {
        // stats->failed_work++;
        spin_unlock(&rf_sink->lock);
        return result;
    }

    // stats->work++;

    // NOTE: Do not unlock the rf_sink's lock - the dma_cb will do it

    return result;
}

void tx_dma_cb(void *data)
{
    struct whitebox_device *wb = (struct whitebox_device *)data;
    struct whitebox_user_source *user_source = &wb->user_source;
    struct whitebox_rf_sink *rf_sink = &wb->rf_sink;
    size_t count;

    count = whitebox_rf_sink_work_done(rf_sink);
    whitebox_user_source_consume(user_source, count);
    whitebox_rf_sink_produce(rf_sink, count);

    spin_unlock(&rf_sink->lock);

    wake_up_interruptible(&wb->write_wait_queue);

    tx_exec(wb);
}

void tx_stop(struct whitebox_device *wb)
{
    wb->rf_sink.exciter->ops->set_state(wb->rf_sink.exciter, WES_TXSTOP);
}

int tx_error(struct whitebox_device *wb)
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
        wb->rf_sink.exciter->ops->get_runs(wb->rf_sink.exciter,
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
    tx_exec(wb);

    return IRQ_HANDLED;
}*/

