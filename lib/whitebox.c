#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "whitebox.h"

void whitebox_init(whitebox_t* wb) {
    wb->fd = -EINVAL;
}

whitebox_t* whitebox_alloc(void) {
    whitebox_t* wb;
    wb = malloc(sizeof(whitebox_t));
    whitebox_init(wb);
    return wb;
}

void whitebox_free(whitebox_t* wb) {
    free(wb);
}

int whitebox_open(whitebox_t* wb, const char* filn, int flags, int rate) {
    char* filename;
    // Can't call open twice

    if (wb->fd >= 0) {
        return -EBUSY;
    }

    if (rate <= 0) {
        return -EINVAL;
    }

    if (filn) {
        filename = strdup(filn);
    }
    else if (getenv("WHITEBOXDEV")) {
        filename = strdup(getenv("WHITEBOXDEV"));
    } else {
        filename = strdup("/dev/whitebox");
    }

    wb->fd = open(filename, flags);


    if (W_DAC_RATE_HZ % rate != 0) {
        fprintf(stderr, "Error, sample rate is not a multiple of DAC clock rate!");
        exit(1);
    }


    wb->rate = rate;
    wb->interp = W_DAC_RATE_HZ / rate;

    whitebox_reset(wb);

    whitebox_tx_set_interp(wb, wb->interp);
    //whitebox_tx_set_buffer_threshold(wb, rate/10, WE_FIFO_SIZE - rate/10);

    free(filename);

    return wb->fd;
}

int whitebox_fd(whitebox_t* wb) {
    return wb->fd;
}

int whitebox_close(whitebox_t* wb) {
    if (wb->fd < 0) {
        return -EBADF;
    }

    close(wb->fd);
    wb->fd = -EINVAL;
    return 0;
}

void whitebox_debug_to_file(whitebox_t* wb, FILE* f) {
    uint32_t value;
    whitebox_args_t w;
    //fprintf(f, "########## EXCITER\n");
    ioctl(wb->fd, WE_GET, &w);
    value = w.flags.exciter.state;
    if (value & WES_DATA) fprintf(f, "data ");
    else fprintf(f, "     ");
    if (value & WES_AFULL) fprintf(f, "afull ");
    else fprintf(f, "     ");
    if (value & WES_SPACE) fprintf(f, "space ");
    else fprintf(f, "      ");
    if (value & WES_AEMPTY) fprintf(f, "aempty ");
    else fprintf(f, "       ");
    if (value & WES_TXEN) fprintf(f, "txen ");
    else fprintf(f, "     ");
    if (value & WES_DDSEN) fprintf(f, "ddsen ");
    else fprintf(f, "      ");
    if (value & WES_FILTEREN) fprintf(f, "filen ");
    else fprintf(f, "      ");
    fprintf(f, "\n");

    /*fprintf(f, "interp=%d\n", w.flags.exciter.interp);
    fprintf(f, "aeval=%d\n", w.flags.exciter.threshold & WET_AEVAL_MASK);
    fprintf(f, "afval=%d\n", (w.flags.exciter.threshold & WET_AFVAL_MASK) >> WET_AFVAL_OFFSET);*/

    /*ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    fprintf(f, "########## CMX991\n");
    cmx991_print_to_file(&wb->cmx991, f);

    ioctl(wb->fd, WA_GET, &w);
    adf4351_ioctl_get(&wb->adf4351, &w);
    fprintf(f, "########## ADF4351\n");
    adf4351_print_to_file(&wb->adf4351, f);*/
}


int whitebox_reset(whitebox_t* wb) {
    if (wb->fd < 0) {
        return -EBADF;
    }
    return ioctl(wb->fd, W_RESET);
}

int whitebox_plls_locked(whitebox_t* wb) {
    return ioctl(wb->fd, W_LOCKED);
}

int whitebox_tx_clear(whitebox_t* wb) {
    if (wb->fd < 0) {
        return -EBADF;
    }
    return ioctl(wb->fd, WE_CLEAR);
}

int whitebox_tx(whitebox_t* wb, float frequency) {
    float vco_frequency;
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_resume(&wb->cmx991);
    if (cmx991_pll_enable_m_n(&wb->cmx991, 19.2e6, 192, 1800) < 0) {
        fprintf(stderr, "Error setting the pll\n");
        return 1;
    }
    cmx991_tx_tune(&wb->cmx991, 198.00e6, IF_FILTER_BW_45MHZ, HI_LO_HIGHER,
        TX_RF_DIV_BY_2, TX_IF_DIV_BY_4, GAIN_P6DB);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);

    vco_frequency = (frequency - 45.00e6) * 2.0;
    if (vco_frequency <= 35.00e6) {
        fprintf(stderr, "VCO frequency too low\n");
        return 2;
    }

    adf4351_init(&wb->adf4351);
    wb->adf4351.charge_pump_current = CHARGE_PUMP_CURRENT_2_50MA;
    wb->adf4351.muxout = MUXOUT_DGND;
    //adf4351.clock_divider_12_bit = 150;
    wb->adf4351.clock_div_mode = CLOCK_DIV_MODE_CLOCK_DIVIDER_OFF;
    wb->adf4351.low_noise_and_spur = LOW_NOISE_AND_SPUR_LOW_NOISE;

    wb->adf4351.rf_output_enable = RF_OUTPUT_ENABLE_ENABLED;
    wb->adf4351.aux_output_power = AUX_OUTPUT_POWER_5DBM;
    wb->adf4351.aux_output_enable = AUX_OUTPUT_ENABLE_ENABLED;
    wb->adf4351.aux_output_select = AUX_OUTPUT_SELECT_DIVIDED;

    adf4351_pll_enable(&wb->adf4351, 26.0e6, 10e3, vco_frequency);
    wb->adf4351.ld_pin_mode = LD_PIN_MODE_DLD;
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);
    return 0;
}

int whitebox_tx_set_interp(whitebox_t* wb, uint32_t interp) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.interp = interp;
    ioctl(wb->fd, WE_SET, &w);
}

int whitebox_tx_set_buffer_threshold(whitebox_t* wb,
            uint16_t aeval, uint16_t afval) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.threshold = (uint32_t)aeval |
                    (uint32_t)(afval << WET_AFVAL_OFFSET);
    ioctl(wb->fd, WE_SET, &w);
}

int whitebox_tx_get_buffer_runs(whitebox_t* wb,
            uint16_t* overruns, uint16_t* underruns) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    *overruns = (uint16_t)((w.flags.exciter.runs & WER_OVERRUNS_MASK)
                    >> WER_OVERRUNS_OFFSET);
    *underruns = (uint16_t)((w.flags.exciter.runs & WER_UNDERRUNS_MASK));
}

void whitebox_tx_set_dds_fcw(whitebox_t* wb, uint32_t fcw) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.fcw = fcw;
    ioctl(wb->fd, WE_SET, &w);
}

void whitebox_tx_flags_enable(whitebox_t* wb, uint32_t flags) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.state |= flags;
    ioctl(wb->fd, WE_SET, &w);
}

void whitebox_tx_flags_disable(whitebox_t* wb, uint32_t flags) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.state &= ~flags;
    ioctl(wb->fd, WE_SET, &w);
}

void whitebox_tx_dds_enable(whitebox_t* wb, float fdes) {
    uint32_t fcw = (uint32_t)(fdes / ((float)W_DAC_RATE_HZ / (float)W_DDS_PA_COUNT));
    whitebox_tx_set_dds_fcw(wb, fcw);
    whitebox_tx_flags_enable(wb, WES_DDSEN);
}
