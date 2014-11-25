#ifndef __WHITEBOX_H__
#define __WHITEBOX_H__

#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>

#include "whitebox_ioctl.h"

#include "cmx991.h"
#include "adf4351.h"
#include "dsp.h"

struct whitebox {
    int fd;
    cmx991_t cmx991;
    adf4351_t adf4351;

    int rate;
    int interp;
    float frequency;

    void *user_buffer;
    unsigned long user_buffer_size;
};

typedef struct whitebox whitebox_t;

long whitebox_tx_bytes_total();
int whitebox_parameter_set(const char *param, int value);
int whitebox_parameter_get(const char *param);

void whitebox_init(whitebox_t* wb);
whitebox_t* whitebox_alloc(void);
void whitebox_free(whitebox_t* wb);
int whitebox_open(whitebox_t* wb, const char* filn, int flags, int rate);
int whitebox_mmap(whitebox_t* wb);
int whitebox_munmap(whitebox_t* wb);
int whitebox_fd(whitebox_t* wb);
int whitebox_close(whitebox_t* wb);
void whitebox_debug_to_file(whitebox_t* wb, FILE* f);
int whitebox_reset(whitebox_t* wb);
unsigned int whitebox_status(whitebox_t* wb);
int whitebox_plls_locked(whitebox_t* wb);

int whitebox_tx_clear(whitebox_t* wb);
int whitebox_tx(whitebox_t* wb, float frequency);
int whitebox_tx_fine_tune(whitebox_t* wb, float frequency);
int whitebox_tx_standby(whitebox_t* wb);

int whitebox_tx_set_interp(whitebox_t* wb, uint32_t interp);
int whitebox_tx_set_buffer_threshold(whitebox_t* wb, uint16_t aeval, uint16_t afval);
void whitebox_tx_get_buffer_threshold(whitebox_t *wb, uint16_t *aeval, uint16_t *afval);
int whitebox_tx_get_buffer_runs(whitebox_t* wb, uint16_t* overruns, uint16_t* underruns);
int whitebox_tx_set_latency(whitebox_t *wb, int ms);
int whitebox_tx_get_latency(whitebox_t *wb);

void whitebox_tx_flags_enable(whitebox_t* wb, uint32_t flags);
void whitebox_tx_flags_disable(whitebox_t* wb, uint32_t flags);

void whitebox_tx_dds_enable(whitebox_t* wb, float fdes);

void whitebox_tx_set_correction(whitebox_t *wb, int16_t correct_i, int16_t correct_q);
void whitebox_tx_get_correction(whitebox_t *wb, int16_t *correct_i, int16_t *correct_q);

int whitebox_tx_set_gain(whitebox_t *wb, float gain_i, float gain_q);
int whitebox_tx_get_gain(whitebox_t *wb, float *gain_i, float *gain_q);


int whitebox_rx_clear(whitebox_t* wb);
int whitebox_rx(whitebox_t* wb, float frequency);
int whitebox_rx_fine_tune(whitebox_t* wb, float frequency);
int whitebox_rx_standby(whitebox_t* wb);
int whitebox_rx_cal_enable(whitebox_t *wb);
int whitebox_rx_cal_disable(whitebox_t *wb);

int whitebox_rx_set_decim(whitebox_t* wb, uint32_t decim);
int whitebox_rx_set_latency(whitebox_t *wb, int ms);
int whitebox_rx_get_latency(whitebox_t *wb);

void whitebox_rx_flags_enable(whitebox_t* wb, uint32_t flags);
void whitebox_rx_flags_disable(whitebox_t* wb, uint32_t flags);

void whitebox_rx_set_correction(whitebox_t *wb, int16_t correct_i, int16_t correct_q);
void whitebox_rx_get_correction(whitebox_t *wb, int16_t *correct_i, int16_t *correct_q);

#endif /* __WHITEBOX_H__ */
