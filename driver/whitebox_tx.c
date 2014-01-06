#include <linux/sched.h>
#include "whitebox.h"
#include "whitebox_gpio.h"
#include "pdma.h"

int tx_start(struct whitebox_device *wb)
{
    struct whitebox_stats *stats = &wb->tx_stats;
    struct whitebox_exciter *exciter = wb->rf_sink.exciter;
    memset(stats, 0, sizeof(struct whitebox_stats));
    exciter->ops->get_runs(exciter, &wb->cur_overruns, &wb->cur_underruns);
    return 0;
}

int tx_exec(struct whitebox_device* wb)
{
    struct whitebox_stats *stats = &wb->tx_stats;
    struct whitebox_user_source *user_source = &wb->user_source;
    struct whitebox_rf_sink *rf_sink = &wb->rf_sink;
    size_t src_count, dest_count;
    unsigned long src, dest;
    size_t count;
    int result;

    stats->exec_calls++;

    if (!spin_trylock(&rf_sink->lock)) {
        stats->exec_busy++;
        return -EBUSY;
    }

    src_count = whitebox_user_source_data_available(user_source, &src);
    dest_count = whitebox_rf_sink_space_available(rf_sink, &dest);
    if (src_count >> 2 == 0) {
        stats->exec_nop_src++;
        spin_unlock(&rf_sink->lock);
        return 0;
    } else if (dest_count >> 2 == 0) {
        stats->exec_nop_dest++;
        spin_unlock(&rf_sink->lock);
        return 0;
    }
    count = min(src_count, dest_count);

    result = whitebox_rf_sink_work(rf_sink, src, src_count, dest, dest_count);

    if (result < 0) {
        stats->exec_failed++;
        spin_unlock(&rf_sink->lock);
        return result;
    }
    else if (result > 0) {
        stats->exec_success_slow++;
        stats->bytes += result;
        whitebox_user_source_consume(user_source, result);
        whitebox_rf_sink_produce(rf_sink, result);
        spin_unlock(&rf_sink->lock);
        wake_up_interruptible(&wb->write_wait_queue);
        tx_exec(wb);
    } else {
        stats->exec_dma_start++;
        // NOTE: Do not unlock the rf_sink's lock if result is 0 as a DMA was
        // started.
    }

    return result;
}

void tx_dma_cb(void *data)
{
    struct whitebox_device *wb = (struct whitebox_device *)data;
    struct whitebox_stats *stats = &wb->tx_stats;
    struct whitebox_user_source *user_source = &wb->user_source;
    struct whitebox_rf_sink *rf_sink = &wb->rf_sink;
    size_t count;

    count = whitebox_rf_sink_work_done(rf_sink);
    whitebox_rf_sink_produce(rf_sink, count);
    whitebox_user_source_consume(user_source, count);

    stats->exec_dma_finished++;
    stats->bytes += count;

    spin_unlock(&rf_sink->lock);

    wake_up_interruptible(&wb->write_wait_queue);

    tx_exec(wb);
}

void tx_stop(struct whitebox_device *wb)
{
    struct whitebox_stats *stats = &wb->tx_stats;
    /*while (pdma_busy(wb->platform_data->tx_dma_ch)) {
        cpu_relax();
    }*/
    stats->stop++;
    wb->rf_sink.exciter->ops->set_state(wb->rf_sink.exciter, WES_TXSTOP);
}

int tx_error(struct whitebox_device *wb)
{
    struct whitebox_stats *stats = &wb->tx_stats;

    if (whitebox_check_plls) {
        int c, locked;
        c = whitebox_gpio_cmx991_read(wb->platform_data,
            WHITEBOX_CMX991_LD_REG);
        locked = whitebox_gpio_adf4351_locked(wb->platform_data)
                && (c & WHITEBOX_CMX991_LD_MASK);
        if (!locked) {
            stats->error++;
            stats->last_error = W_ERROR_PLL_LOCK_LOST;
            return W_ERROR_PLL_LOCK_LOST;
        }
    }

    if (whitebox_check_runs) {
        u16 overruns, underruns;
        wb->rf_sink.exciter->ops->get_runs(wb->rf_sink.exciter,
                &overruns, &underruns);
        if (wb->cur_overruns != overruns) {
            wb->cur_overruns = overruns;
            stats->error++;
            stats->last_error = W_ERROR_TX_OVERRUN;
            return W_ERROR_TX_OVERRUN;
        }
        if (wb->cur_underruns != underruns) {
            wb->cur_underruns = underruns;
            stats->error++;
            stats->last_error = W_ERROR_TX_UNDERRUN;
            return W_ERROR_TX_UNDERRUN;
        }
    }
    return 0;
}

/*static irqreturn_t tx_irq_cb(int irq, void* ptr) {
    struct whitebox_device* wb = (struct whitebox_device*)ptr;
    tx_exec(wb);

    return IRQ_HANDLED;
}*/

