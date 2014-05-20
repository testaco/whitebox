#include <stdio.h>
#include <stdlib.h>
#include "whiteboxd.h"

int whitebox_source_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    return source->ops->work(source, src, src_count, dest, dest_count);
}

void whitebox_source_free(struct whitebox_source *source)
{
    free(source);
}

int poll_none(struct whitebox_source *source,
        struct pollfd* fds)
{
    fds->fd = -1;
    fds->events = 0;
    return 0;
}

/* SYNTHESIZERS */

int whitebox_source_tune(struct whitebox_source *source, float fdes)
{
    struct whitebox_synth_source *synth =
        container_of(source, struct whitebox_synth_source, source);
    synth->fcw = freq_to_fcw(fdes, 48e3);
    synth->phase = 0;
    synth->freq = fdes;
    return 0;
}

int32_t qsynth_lsb_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    struct whitebox_synth_source *synth =
        container_of(source, struct whitebox_synth_source, source);
    int n;
    int16_t re, im;
    /*for (n = 0; n < dest_count >> 2; ++n) {
        sincos16(synth->fcw, &synth->phase, &re, &im);
        *(((uint32_t*)dest) + n) = QUAD_PACK(re >> 1, im >> 1);
    }*/
    for (n = 0; n < dest_count >> 2; ++n) {
        QUAD_UNPACK(sincos16c(synth->fcw, &synth->phase), re, im);
        *(((uint32_t*)dest) + n) = QUAD_PACK(re, im);
    }
    return dest_count;
}

struct whitebox_source_operations qsynth_lsb_ops = {
    .poll = poll_none,
    .work = qsynth_lsb_work,
};

int whitebox_qsynth_lsb_alloc(struct whitebox_source **source)
{
    *source = malloc(sizeof(struct whitebox_synth_source));
    (*source)->ops = &qsynth_lsb_ops;
    return 0;
}

int32_t qsynth_usb_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    struct whitebox_synth_source *synth =
        container_of(source, struct whitebox_synth_source, source);
    int n;
    int16_t re, im;
    /*for (n = 0; n < dest_count >> 2; ++n) {
        sincos16(synth->fcw, &synth->phase, &re, &im);
        *(((uint32_t*)dest) + n) = QUAD_PACK(re >> 1, im >> 1);
    }*/
    for (n = 0; n < dest_count >> 2; ++n) {
        QUAD_UNPACK(sincos16c(synth->fcw, &synth->phase), re, im);
        *(((uint32_t*)dest) + n) = QUAD_PACK(re, -im);
    }
    return dest_count;
}

struct whitebox_source_operations qsynth_usb_ops = {
    .poll = poll_none,
    .work = qsynth_usb_work,
};

int whitebox_qsynth_usb_alloc(struct whitebox_source **source)
{
    *source = malloc(sizeof(struct whitebox_synth_source));
    (*source)->ops = &qsynth_usb_ops;
    return 0;
}

int32_t qconst_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    struct whitebox_const_source *const_source =
        container_of(source, struct whitebox_const_source, source);
    uint32_t val = QUAD_PACK(const_source->re, const_source->im);
    int n;
    for (n = 0; n < dest_count >> 2; ++n)
        *(((uint32_t*)dest) + n) = val;
    return dest_count;
}

struct whitebox_source_operations qconst_ops = {
    .poll = poll_none,
    .work = qconst_work,
};

int whitebox_qconst_alloc(struct whitebox_source **source)
{
    *source = malloc(sizeof(struct whitebox_const_source));
    (*source)->ops = &qconst_ops;
    return 0;
}

/* FILE */


int poll_file_read(struct whitebox_source *source,
        struct pollfd* fds)
{
    struct whitebox_file_source *fsource =
            container_of(source, struct whitebox_file_source, source);
    fds->fd = fsource->fd;
    fds->events = POLLIN;
    return 0;
}

int32_t qsocket_work(struct whitebox_source *source,
        unsigned long src, size_t src_count,
        unsigned long dest, size_t dest_count)
{
    struct whitebox_file_source *fsource =
        container_of(source, struct whitebox_file_source, source);
    return dest_count;
}

struct whitebox_source_operations qsocket_ops = {
    .poll = poll_file_read,
    .work = qsocket_work,
};
