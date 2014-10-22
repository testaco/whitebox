#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>

#include "whitebox.h"

int whitebox_parameter_set(const char *param, int value)
{
    char name[512];
    char final_value[128];
    int fd;
    snprintf(name, 512, "/sys/module/whitebox/parameters/whitebox_%s", param);
    snprintf(final_value, 128, "%d\n", value);
    fd = open(name, O_WRONLY);
    if (fd < 0) {
        perror("open");
        return fd;
    }
    if (write(fd, final_value, strlen(final_value)+1) < 0) {
        perror("write");
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

int whitebox_parameter_get(const char *param)
{
    char name[512];
    char final_value[128];
    int fd;
    snprintf(name, 512, "/sys/module/whitebox/parameters/whitebox_%s", param);
    fd = open(name, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return fd;
    }
    if (read(fd, &final_value, 127) < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    close(fd);
    return atoi(final_value);
}


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
    whitebox_rx_set_decim(wb, wb->interp);
    //whitebox_tx_set_buffer_threshold(wb, rate/10, WE_FIFO_SIZE - rate/10);

    free(filename);

    return wb->fd;
}

int whitebox_mmap(whitebox_t* wb) {
    wb->user_buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    wb->user_buffer = mmap(0, wb->user_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, wb->fd, 0);
    if (wb->user_buffer == MAP_FAILED || !wb->user_buffer)
        return -1;
    return 0;
}

int whitebox_munmap(whitebox_t *wb) {
    return munmap(wb->user_buffer, wb->user_buffer_size);
}

int whitebox_fd(whitebox_t* wb) {
    return wb->fd;
}

int whitebox_close(whitebox_t* wb) {
    if (wb->fd < 0) {
        return -EBADF;
    }
    whitebox_args_t w;
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_suspend(&wb->cmx991);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);

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
    if (value & WES_FILTEREN) fprintf(f, "cicen ");
    else fprintf(f, "      ");
    if (value & WS_FIREN) fprintf(f, "firen ");
    else fprintf(f, "      ");
    fprintf(f, "space=%d ", w.flags.exciter.available);
    fprintf(f, "debug=%08x ", w.flags.exciter.debug);
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
    int i;
    // We wait for a little bit incase tuning just changed
    for (i = 0; i < 1000; ++i) {
        if (ioctl(wb->fd, W_LOCKED))
            return 1;
    }
    return 0;
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
#if WC_USE_PLL
    if (cmx991_pll_enable_m_n(&wb->cmx991, 19.2e6, 192, 1800) < 0) {
        fprintf(stderr, "Error setting the pll\n");
        return 1;
    }
#endif

    vco_frequency = (frequency + 45.00e6) * 4.0;
    if (vco_frequency <= 35.00e6) {
        fprintf(stderr, "VCO frequency too low\n");
        return 2;
    }
    //printf("%f %f\n", frequency, vco_frequency);

    cmx991_tx_tune(&wb->cmx991, vco_frequency,
        IF_FILTER_BW_45MHZ, HI_LO_LOWER,
        TX_RF_DIV_BY_4, TX_IF_DIV_BY_4, GAIN_P6DB);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);

    adf4351_init(&wb->adf4351);

    adf4351_pll_enable(&wb->adf4351, WA_CLOCK_RATE, 8e3, vco_frequency);
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);

    ioctl(wb->fd, WE_CLEAR, 0);
    whitebox_tx_flags_enable(wb, WES_FILTEREN);
    return 0;
}

int whitebox_tx_standby(whitebox_t *wb)
{
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_suspend(&wb->cmx991);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);
}

int whitebox_tx_fine_tune(whitebox_t *wb, float frequency) {
    float vco_frequency;
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_resume(&wb->cmx991);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);

    vco_frequency = (frequency + 45.00e6) * 4.0;
    if (vco_frequency <= 35.00e6) {
        fprintf(stderr, "VCO frequency too low\n");
        return 2;
    }

    adf4351_pll_enable(&wb->adf4351, WA_CLOCK_RATE, 8e3, vco_frequency);
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);
}

uint16_t _cic_shift(uint16_t interp)
{
    // Based on the excellent work of Hogenauer, 1981.
    float cic_order = 4, cic_delay = 1, in_len=9, out_len=10; // From CIC implementation on the FPGA
    float stage = 2 * cic_order; // Last stage
    float gain = (pow(2, 2 * cic_order - stage) * pow(interp * cic_delay, stage - cic_order)) / interp;
    float bit_width = in_len + ceil(log(gain)/log(2));
    float shift = bit_width - out_len;
    return (shift < 0) ? 0 : (uint16_t)shift;
}

int whitebox_tx_set_interp(whitebox_t* wb, uint32_t interp) {
    uint16_t shift;
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.interp = (_cic_shift(interp) << 16) & 0xffff0000 | (interp & 0xffff);
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

void whitebox_tx_get_buffer_threshold(whitebox_t *wb,
            uint16_t *aeval, uint16_t *afval)
{
    whitebox_args_t w;            
    ioctl(wb->fd, WE_GET, &w);
    *aeval = (uint16_t)(w.flags.exciter.threshold & WET_AEVAL_MASK);
    *afval = (uint16_t)((w.flags.exciter.threshold & WET_AFVAL_MASK) >> WET_AFVAL_OFFSET);
}

int whitebox_tx_get_buffer_runs(whitebox_t* wb,
            uint16_t* overruns, uint16_t* underruns) {
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    *overruns = (uint16_t)((w.flags.exciter.runs & WER_OVERRUNS_MASK)
                    >> WER_OVERRUNS_OFFSET);
    *underruns = (uint16_t)((w.flags.exciter.runs & WER_UNDERRUNS_MASK));
}

int whitebox_tx_set_latency(whitebox_t *wb, int ms)
{
    int threshold = 4 * wb->rate * ((float)ms * 1e-3);
    return whitebox_parameter_set("user_source_buffer_threshold", threshold);
}

int whitebox_tx_get_latency(whitebox_t *wb)
{
    int threshold = whitebox_parameter_get("user_source_buffer_threshold");
    if (threshold <= 0)
        return threshold;
    int latency_ms = (int)(threshold / (wb->rate * 4 * 1e-3));
    return latency_ms;
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
    w.flags.exciter.state = flags;
    ioctl(wb->fd, WE_CLEAR_MASK, &w);
}

void whitebox_tx_dds_enable(whitebox_t* wb, float fdes) {
    uint32_t fcw = (uint32_t)(fdes / ((float)W_DAC_RATE_HZ / (float)W_DDS_PA_COUNT));
    whitebox_tx_set_dds_fcw(wb, fcw);
    whitebox_tx_flags_enable(wb, WES_DDSEN);
}

void whitebox_tx_set_correction(whitebox_t *wb, int16_t correct_i, int16_t correct_q)
{
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.correction = (uint32_t)(((int32_t)correct_i & WEC_I_MASK) |
                        (((int32_t)correct_q << WEC_Q_OFFSET) & WEC_Q_MASK));
    ioctl(wb->fd, WE_SET, &w);
}

void whitebox_tx_get_correction(whitebox_t *wb, int16_t *correct_i, int16_t *correct_q)
{
    whitebox_args_t w;
    ioctl(wb->fd, WE_GET, &w);
    *correct_i = ((int16_t)(w.flags.exciter.correction & WEC_I_MASK)) << 6;
    *correct_i >>= 6;
    *correct_q = (int16_t)(((w.flags.exciter.correction & WEC_Q_MASK)) >> WEC_Q_OFFSET) << 6;
    *correct_q >>= 6;
}

int whitebox_tx_set_gain(whitebox_t *wb, float gain_i, float gain_q)
{
    whitebox_args_t w;
    uint32_t gi, gq, newg;

    /*if (!(gain_i >= 0.0 && gain_i < 2.0))
        return -EINVAL;
    if (!(gain_q >= 0.0 && gain_q < 2.0))
        return -EINVAL;*/

    gi = (uint32_t)(gain_i * WEG_COEFF + 0.5);
    gq = (uint32_t)(gain_q * WEG_COEFF + 0.5);
    newg = (uint32_t)((gi & WEG_I_MASK) |
            ((gq << WEG_Q_OFFSET) & WEG_Q_MASK));

    ioctl(wb->fd, WE_GET, &w);
    w.flags.exciter.gain = newg;
    return ioctl(wb->fd, WE_SET, &w);
}

int whitebox_tx_get_gain(whitebox_t *wb, float *gain_i, float *gain_q)
{
    whitebox_args_t w;
    uint32_t gi, gq;
    ioctl(wb->fd, WE_GET, &w);
    gi = ((uint32_t)(w.flags.exciter.gain & WEG_I_MASK));
    *gain_i = gi / WEG_COEFF;
    gq = (uint32_t)(((w.flags.exciter.gain & WEC_Q_MASK)) >> WEC_Q_OFFSET);
    *gain_q = gq / WEG_COEFF;
    return 0;
}

int whitebox_rx_clear(whitebox_t* wb) {
    if (wb->fd < 0) {
        return -EBADF;
    }
    return ioctl(wb->fd, WR_CLEAR);
}

int whitebox_rx(whitebox_t* wb, float frequency) {
    float vco_frequency;
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_resume(&wb->cmx991);
#if WC_USE_PLL
    if (cmx991_pll_enable_m_n(&wb->cmx991, 19.2e6, 192, 1800) < 0) {
        fprintf(stderr, "Error setting the pll\n");
        return 1;
    }
#endif

    vco_frequency = (frequency + 45.00e6) * 4.0;
    if (vco_frequency <= 35.00e6) {
        fprintf(stderr, "VCO frequency too low\n");
        return 2;
    }

    //printf("%f %f\n", frequency, vco_frequency);
    cmx991_rx_tune(&wb->cmx991, //vco_frequency,
        RX_RF_DIV_BY_4, MIX_OUT_MIXOUT1, IF_IN_IFIP1,
        IQ_FILTER_BW_1MHZ, VGA_N0DB);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);

    adf4351_init(&wb->adf4351);

    adf4351_pll_enable(&wb->adf4351, WA_CLOCK_RATE, 8e3, vco_frequency);
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);
    return 0;
}

int whitebox_rx_fine_tune(whitebox_t *wb, float frequency) {
    float vco_frequency;
    whitebox_args_t w;

    vco_frequency = (frequency + 45.00e6) * 4.0;
    if (vco_frequency <= 35.00e6) {
        fprintf(stderr, "VCO frequency too low\n");
        return 2;
    }

    adf4351_pll_enable(&wb->adf4351, WA_CLOCK_RATE, 8e3, vco_frequency);
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);
}

int whitebox_rx_standby(whitebox_t *wb)
{
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_suspend(&wb->cmx991);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);
    return 0;
}

int whitebox_rx_cal_enable(whitebox_t *wb)
{
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_rx_calibrate_enable(&wb->cmx991);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);
    return 0;
}

int whitebox_rx_cal_disable(whitebox_t *wb)
{
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_rx_calibrate_disable(&wb->cmx991);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);
    return 0;
}

int whitebox_rx_set_decim(whitebox_t* wb, uint32_t decim) {
    whitebox_args_t w;
    if (ioctl(wb->fd, WR_GET, &w) < 0)
        return -1;
    w.flags.receiver.decim = decim;
    if (ioctl(wb->fd, WR_SET, &w) < 0)
        return -1;
    return 0;
}

int whitebox_rx_set_latency(whitebox_t *wb, int ms)
{
    int threshold = 4 * wb->rate * ((float)ms * 1e-3);
    return whitebox_parameter_set("user_sink_buffer_threshold", threshold);
}

int whitebox_rx_get_latency(whitebox_t *wb)
{
    int threshold = whitebox_parameter_get("user_sink_buffer_threshold");
    if (threshold <= 0)
        return threshold;
    int latency_ms = (int)(threshold / (wb->rate * 4 * 1e-3));
    return latency_ms;
}

void whitebox_rx_flags_enable(whitebox_t* wb, uint32_t flags) {
    whitebox_args_t w;
    ioctl(wb->fd, WR_GET, &w);
    w.flags.receiver.state |= flags;
    ioctl(wb->fd, WR_SET, &w);
}

void whitebox_rx_flags_disable(whitebox_t* wb, uint32_t flags) {
    whitebox_args_t w;
    w.flags.receiver.state = flags;
    ioctl(wb->fd, WR_CLEAR_MASK, &w);
}

void whitebox_rx_set_correction(whitebox_t *wb, int16_t correct_i, int16_t correct_q)
{
    whitebox_args_t w;
    ioctl(wb->fd, WR_GET, &w);
    w.flags.receiver.correction = (uint32_t)(((int32_t)correct_i & WEC_I_MASK) |
                        (((int32_t)correct_q << WEC_Q_OFFSET) & WEC_Q_MASK));
    ioctl(wb->fd, WR_SET, &w);
}

void whitebox_rx_get_correction(whitebox_t *wb, int16_t *correct_i, int16_t *correct_q)
{
    whitebox_args_t w;
    ioctl(wb->fd, WR_GET, &w);
    *correct_i = ((int16_t)(w.flags.receiver.correction & WEC_I_MASK)) << 6;
    *correct_i >>= 6;
    *correct_q = (int16_t)(((w.flags.receiver.correction & WEC_Q_MASK)) >> WEC_Q_OFFSET) << 6;
    *correct_q >>= 6;
}

int whitebox_fir_load_coeffs(whitebox_t *wb, int8_t bank, int8_t N, int32_t *coeffs)
{
    int i;
    whitebox_args_t w;
    w.flags.fir.bank = bank;
    w.flags.fir.n = N;
    for (i = 0; i < N; ++i)
        w.flags.fir.coeff[i] = coeffs[i];

    ioctl(wb->fd, WF_SET, &w);
    return 0;
}

int whitebox_fir_get_coeffs(whitebox_t *wb, int8_t bank, int8_t N, int32_t *coeffs)
{
    int i;
    int n;
    whitebox_args_t w;
    ioctl(wb->fd, WF_GET, &w);
    n = w.flags.fir.n < N ? w.flags.fir.n : N;
    
    for (i = 0; i < n; ++i)
        coeffs[i] = w.flags.fir.coeff[i];

    return w.flags.fir.n;
}
