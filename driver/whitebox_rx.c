#include <linux/sched.h>
#include "whitebox.h"
#include "whitebox_gpio.h"
#include "pdma.h"

int tx_exec(struct whitebox_device* wb);

// ARM Cortex-M3 SysTick interface
extern int *STCSR;
extern int *STRVR;
extern int *STCVR;

int rx_start(struct whitebox_device *wb)
{
    struct whitebox_receiver *receiver = wb->rf_source.receiver;
    receiver->ops->get_runs(receiver, &wb->cur_overruns, &wb->cur_underruns);
    pdma_clear(wb->platform_data->rx_dma_ch);

    // Reset the SysTick interface
    *STRVR = 0xFFFFFF;
    *STCVR = 0;
    *STCSR = 5;
    return 0;
}

int rx_exec(struct whitebox_device* wb)
{
    struct whitebox_stats *stats = &wb->rx_stats;
    struct whitebox_stats_exec_detail *stats_detail;
    struct whitebox_rf_source *rf_source = &wb->rf_source;
    struct whitebox_user_sink *user_sink = &wb->user_sink;
    size_t src_count, dest_count;
    unsigned long src, dest;
    size_t count;
    int result;
    unsigned long flags;

    stats->exec_calls++;

    if (!spin_trylock_irqsave(&rf_source->lock, flags)) {
        stats->exec_busy++;
        return -EBUSY;
    }

    stats_detail = &stats->exec_detail[stats->exec_detail_index];
    stats_detail->time = *STCVR; // Get the current SysTick count

    src_count = whitebox_rf_source_data_available(rf_source, &src);
    dest_count = whitebox_user_sink_space_available(user_sink, &dest);
    if (src_count >> 2 == 0) {
        stats->exec_nop_src++;
        spin_unlock_irqrestore(&rf_source->lock, flags);
        return 0;
    } else if (dest_count >> 2 == 0) {
        stats->exec_nop_dest++;
        spin_unlock_irqrestore(&rf_source->lock, flags);
        return 0;
    }
    count = min(src_count, dest_count);

    result = whitebox_rf_source_work(rf_source, src, src_count, dest, dest_count);
    stats_detail->src = src_count;
    stats_detail->dest = dest_count;
    stats_detail->bytes = count;
    stats_detail->result = result;
    stats->exec_detail_index = (stats->exec_detail_index + 1) & (W_EXEC_DETAIL_COUNT - 1);

    if (result < 0) {
        stats->exec_failed++;
        spin_unlock_irqrestore(&rf_source->lock, flags);
        return result;
    }

    //whitebox_user_sink_produce(user_sink, count);
    whitebox_rf_source_consume(rf_source, count);

    if (result > 0) {
        stats->exec_success_slow++;
        stats->bytes += result;
        whitebox_user_sink_produce(user_sink, result);
        //whitebox_rf_source_consume(rf_source, result);
        spin_unlock_irqrestore(&rf_source->lock, flags);
        wake_up_interruptible(&wb->read_wait_queue);
        if (whitebox_loopen) {
            wake_up_interruptible(&wb->write_wait_queue);
            tx_exec(wb);
        }
        rx_exec(wb);
    } else {
        stats->exec_dma_start++;
        spin_unlock_irqrestore(&rf_source->lock, flags);
    }

    return result;
}

void rx_dma_cb(void *data, int buf)
{
    struct whitebox_device *wb = (struct whitebox_device *)data;
    struct whitebox_stats *stats = &wb->rx_stats;
    struct whitebox_rf_source *rf_source = &wb->rf_source;
    struct whitebox_user_sink *user_sink = &wb->user_sink;
    size_t count;
    unsigned long flags;

    spin_lock_irqsave(&rf_source->lock, flags);

    count = whitebox_rf_source_work_done(rf_source, buf);

    whitebox_user_sink_produce(user_sink, count);

    stats->exec_dma_finished++;
    stats->bytes += count;

    spin_unlock_irqrestore(&rf_source->lock, flags);

    rx_exec(wb);

    wake_up_interruptible(&wb->read_wait_queue);
    if (whitebox_loopen) {
        wake_up_interruptible(&wb->write_wait_queue);
        tx_exec(wb);
    }
}

int rx_stop(struct whitebox_device *wb)
{
    struct whitebox_stats *stats = &wb->rx_stats;
    stats->stop++;

    if (pdma_buffers_available(wb->platform_data->rx_dma_ch) < 2)
        return -1;

    wb->rf_source.receiver->ops->set_state(wb->rf_source.receiver, WRS_RXSTOP);
    return 0;
}

void d_printk_loop(int level);
int rx_error(struct whitebox_device *wb)
{
    struct whitebox_stats *stats = &wb->rx_stats;

    if (whitebox_check_plls) {
        int c, locked;
#if WC_USE_PLL
        c = whitebox_gpio_cmx991_read(wb->platform_data,
            WHITEBOX_CMX991_LD_REG);
#else
        c = WHITEBOX_CMX991_LD_REG;
#endif
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
        wb->rf_source.receiver->ops->get_runs(wb->rf_source.receiver,
                &overruns, &underruns);

        if (wb->cur_overruns != overruns) {
            wb->cur_overruns = overruns;
            stats->error++;
            stats->last_error = W_ERROR_RX_OVERRUN;
            return W_ERROR_RX_OVERRUN;
        }
        if (wb->cur_underruns != underruns) {
            wb->cur_underruns = underruns;
            stats->error++;
            stats->last_error = W_ERROR_RX_OVERRUN;
            return W_ERROR_RX_OVERRUN;
        }
    }
    d_printk_loop(4);
    return 0;
}

/*static irqreturn_t tx_irq_cb(int irq, void* ptr) {
    struct whitebox_device* wb = (struct whitebox_device*)ptr;
    rx_exec(wb);

    return IRQ_HANDLED;
}*/

