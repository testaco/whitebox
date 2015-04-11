#ifndef __WHITEBOXD_H__
#define __WHITEBOXD_H__

#include "whitebox.h"
#include "http.h"
#include <poll.h>

struct whitebox_source_operations;

struct whitebox_source {
    struct whitebox_source_operations *ops;    
};

struct whitebox_source_operations {
    int (*poll)(struct whitebox_source *source,
            struct pollfd* fds);
    int (*work)(struct whitebox_source *source,
            unsigned long src, size_t src_count,
            unsigned long dest, size_t dest_count);
};

int whitebox_source_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count);
void whitebox_source_free(struct whitebox_source *source);

struct whitebox_synth_source {
    struct whitebox_source source;
    float freq;
    uint32_t fcw;
    uint32_t phase;
};

struct whitebox_const_source {
    struct whitebox_source source;
    int16_t re;
    int16_t im;
};

int whitebox_source_tune(struct whitebox_source *source, float fdes);

int whitebox_const(struct whitebox_source *source);
int whitebox_synth(struct whitebox_source *source);
int whitebox_socket(struct whitebox_source *source);

int whitebox_qsynth_lsb_alloc(struct whitebox_source **source);
int whitebox_qsynth_usb_alloc(struct whitebox_source **source);
int whitebox_qconst_alloc(struct whitebox_source **source);
int whitebox_qsocket_alloc(struct whitebox_source **source);

struct whitebox_file_source {
    struct whitebox_source source;
    int fd;
};

int (*modulate)(struct whitebox_source *iq_source,
        struct whitebox_source *audio_source);

int modulate_idle(struct whitebox_source *iq_source,
        struct whitebox_source *audio_source);

#endif /* __WHITEBOXD_H__ */
