#ifndef __WHITEBOX_H__
#define __WHITEBOX_H__

#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>

#include "whitebox_ioctl.h"

#include "cmx991.h"
#include "adf4351.h"

typedef struct whitebox {
    int fd;
    cmx991_t cmx991;
    adf4351_t adf4351;

    int rate;
    int interp;
    float frequency;
} whitebox_t;

void whitebox_init(whitebox_t* wb);
whitebox_t* whitebox_alloc(void);
void whitebox_free(whitebox_t* wb);
int whitebox_open(whitebox_t* wb, const char* filn, int flags, int rate);
int whitebox_fd(whitebox_t* wb);
int whitebox_close(whitebox_t* wb);
void whitebox_debug_to_file(whitebox_t* wb, FILE* f);
int whitebox_reset(whitebox_t* wb);
unsigned int whitebox_status(whitebox_t* wb);
int whitebox_plls_locked(whitebox_t* wb);

int whitebox_tx_clear(whitebox_t* wb);
int whitebox_tx(whitebox_t* wb, float frequency);
int whitebox_tx_set_interp(whitebox_t* wb, uint32_t interp);
int whitebox_tx_set_buffer_threshold(whitebox_t* wb, uint16_t aeval, uint16_t afval);
int whitebox_tx_get_buffer_runs(whitebox_t* wb, uint16_t* overruns, uint16_t* underruns);
int whitebox_tx_get_ring_buffer_size(whitebox_t* wb);

void whitebox_tx_flags_enable(whitebox_t* wb, uint32_t flags);
void whitebox_tx_flags_disable(whitebox_t* wb, uint32_t flags);

#endif /* __WHITEBOX_H__ */
