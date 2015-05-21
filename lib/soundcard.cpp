#include <alsa/asoundlib.h>

#include "soundcard.h"

char *device = 0;
unsigned int rate = 48000;
unsigned int playback_channels = 2;
unsigned int capture_channels = 1;
unsigned int buffer_time = 500000;
unsigned int period_time = 100000;

static snd_pcm_format_t format = SND_PCM_FORMAT_S16;
static snd_pcm_uframes_t buffer_size;
static snd_pcm_uframes_t period_size;

// Utility function for setting up an alsa device
int configure_alsa_pcm(snd_pcm_t *handle, unsigned int nch) {
    unsigned int pcm;
    unsigned int tmp;
    int dir;
    snd_pcm_hw_params_t *params;
    
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);

    if (pcm = snd_pcm_hw_params_set_access(handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        //fprintf(stderr, "Can't set interleaved mode. %s\n",
        //    snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params_set_format(handle, params,
            format) < 0) {
        //fprintf(stderr, "Can't set format. %s\n",
        //    snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params_set_channels(handle, params,
            nch) < 0) {
        //fprintf(stderr, "Can't set channels. %s\n",
        //    snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params_set_rate_near(handle, params,
            &rate, 0) < 0) {
        //fprintf(stderr, "Can't set rate. %s\n",
        //    snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params(handle, params) < 0) {
        //fprintf(stderr, "Can't set hw params. %s\n",
        //    snd_strerror(pcm));
        return -1;
    }

    //fprintf(stdout, "PCM name: '%s'\n", snd_pcm_name(handle));
    //fprintf(stdout, "PCM state: '%s'\n", snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_channels(params, &tmp);
    //fprintf(stdout, "channels: %d\n", tmp);

    snd_pcm_hw_params_get_rate(params, &tmp, 0);
    //fprintf(stdout, "rate: %d\n", rate);

    //fprintf(stdout, "format width: %d\n", snd_pcm_format_physical_width(format));

    if ((pcm = snd_pcm_hw_params_get_period_size(params, &period_size, &dir)) < 0) {
        //fprintf(stderr, "Can't get period size. %s\n",
        //    snd_strerror(pcm));
    }

    return 0;
}

int xrun_recovery(snd_pcm_t *handle, int err) {
    //fprintf(stdout, "Stream recovery\n");
    if (err == -EPIPE) {
        err = snd_pcm_prepare(handle);
        if (err < 0) {
            //fprintf(stderr, "Cant recover from underrun: %s\n", snd_strerror(err));
        }
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0) {
                //fprintf(stderr, "Cant recover from underrun: %s\n", snd_strerror(err));
            }
        return 0;
        }
    }
    return err;
}

int alsa_resource_recover(
        snd_pcm_t *handle) {
    int err;
    if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ||
        snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED) {
            err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ?
                -EPIPE : -ESTRPIPE;
        if (xrun_recovery(handle, err) < 0) {
            //fprintf(stderr, "Write error: %s\n", snd_strerror(err));
            exit(-1);
        }
    }
}

int playback_transfer(
        struct playback *playback) {
    static int init = 1;
    int err, cptr;
    signed short *ptr;

    const snd_pcm_channel_area_t *areas = playback->areas;
    snd_pcm_uframes_t offset = 0;
    int size = period_size;
    unsigned char *samples[playback_channels];
    int steps[playback_channels];
    unsigned int chn;
    int res = 0;
    int16_t ri, rq;
    int i;
    int format_bits = snd_pcm_format_width(format);
    int bps = format_bits / 8;

    // Make sure that the channel areas are ligned up correctly, and
    // figure out the offsets to write to.
    for (chn = 0; chn < playback_channels; ++chn) {
        if ((areas[chn].first % 8) != 0) {
            //printf("boo\n");
            exit(-1);
        }
        samples[chn] = (((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
        if ((areas[chn].step % 16) != 0) {
            //printf("boo2\n");
            exit(-1);
        }
        steps[chn] = areas[chn].step / 8;
        samples[chn] += offset * steps[chn];
    }

    // Fill in the buffer from the source.
    while (size-- > 0) {
        res = (int)source();

        for (chn = 0; chn < playback_channels; ++chn) {
            for (i = 0; i < bps; ++i)
                *(samples[chn] + i) = (res >> i * 8) & 0xff;
            samples[chn] += steps[chn];
        }
    }

    // Notify the sound PCM device that data was written; recover from
    // xruns as necessary.
    ptr = playback->samples;
    cptr = period_size;
    while (cptr > 0) {
        err = snd_pcm_writei(playback->handle, ptr, cptr);
        if (err < 0) {
            if (xrun_recovery(playback->handle, err) < 0) {
                //printf("Write error: %s\n", snd_strerror(err));
            }
            init = 1;
            break;
        }
        if (snd_pcm_state(playback->handle) == SND_PCM_STATE_RUNNING)
            init = 0;
        ptr += err * playback_channels;
        cptr -= err;
        if (cptr == 0)
            break;
    }
}

int capture_transfer(
        struct capture *capture) {
    static int init = 1;
    int err, cptr;
    signed short *ptr;

    const snd_pcm_channel_area_t *areas = capture->areas;
    snd_pcm_uframes_t offset = 0;
    int size = period_size;
    unsigned char *samples[capture_channels];
    int steps[capture_channels];
    unsigned int chn;
    int res = 0;
    int16_t ri, rq;
    int i;
    int format_bits = snd_pcm_format_width(format);
    int bps = format_bits / 8;

    // Make sure that the channel areas are ligned up correctly, and
    // figure out the offsets to write to.
    for (chn = 0; chn < capture_channels; ++chn) {
        if ((areas[chn].first % 8) != 0) {
            //printf("boo\n");
            exit(-1);
        }
        samples[chn] = (((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
        if ((areas[chn].step % 16) != 0) {
            //printf("boo2\n");
            exit(-1);
        }
        steps[chn] = areas[chn].step / 8;
        samples[chn] += offset * steps[chn];
    }

    // Notify the sound PCM device that data was read; recover from
    // xruns as necessary.
    ptr = capture->samples;
    cptr = period_size;
    while (cptr > 0) {
        err = snd_pcm_readi(capture->handle, ptr, cptr);
        if (err < 0) {
            if (xrun_recovery(capture->handle, err) < 0) {
                //printf("Read error: %s\n", snd_strerror(err));
            }
            init = 1;
            break;
        }
        if (snd_pcm_state(capture->handle) == SND_PCM_STATE_RUNNING)
            init = 0;
        ptr += err * capture_channels;
        cptr -= err;
        if (cptr == 0)
            break;
    }

    // Fill in the sink buffer; only use the first mic channel for now;
    while (size-- > 0) {
        res = 0;
        for (i = 0; i < bps; ++i) {
            res |= *(samples[0] + i) << (i * 8);
        }
        samples[0] += steps[0];
        
        sink((int16_t)res);
        
        for (chn = 1; chn < capture_channels; ++chn) {
            for (i = 0; i < bps; ++i)
                *(samples[chn] + i) = 0;
            samples[chn] += steps[chn];
        }
    }
}


void *playback_init() {
    struct playback *playback;
    unsigned int chn;
    unsigned int err, tmp, dir;

    playback = (struct playback*) malloc(sizeof(struct playback));
    if (playback == NULL) {
        //fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }

    if (err = snd_pcm_open(&playback->handle, device,
            SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        //fprintf(stderr, "Can't open '%s' PCM device. %s\n",
        //    device, snd_strerror(err));
        return NULL;
    }

    configure_alsa_pcm(playback->handle, playback_channels);

    playback->samples = (int16_t*)malloc((period_size * playback_channels * snd_pcm_format_physical_width(format)) / 8);
    if (playback->samples == NULL) {
        //fprintf(stderr, "1not enough memory\n");
        return NULL;
    }

    playback->areas = (snd_pcm_channel_area_t*)calloc(playback_channels, sizeof(snd_pcm_channel_area_t));
    if (playback->areas == NULL) {
        //fprintf(stderr, "2not enough memory\n");
        return NULL;
    }

    for (chn = 0; chn < playback_channels; ++chn) {
        playback->areas[chn].addr = playback->samples;
        playback->areas[chn].first = chn * snd_pcm_format_physical_width(format);
        playback->areas[chn].step = playback_channels * snd_pcm_format_physical_width(format);
    }
    return playback;
}

void playback_close(void *data) {
    struct playback *playback = (struct playback *)data;
    free(playback->areas);
    free(playback->samples);
    snd_pcm_close(playback->handle);
    free(playback);
}

int playback_descriptors_count(void *data) {
    struct playback *playback = (struct playback *)data;
    return snd_pcm_poll_descriptors_count(playback->handle);
}

int playback_descriptors(void *data, struct pollfd *ufds, int count) {
    struct playback *playback = (struct playback *)data;
    int err;
    if ((err = snd_pcm_poll_descriptors(playback->handle,
            ufds, count)) < 0) {
        //printf("Unable to get playback descriptors: %s\n", snd_strerror(err));
        return err;
    }
    return count;
}

void playback_handler(void *data, struct pollfd *ufds, int count) {
    struct playback *playback = (struct playback *)data;
    unsigned short revents;
    snd_pcm_poll_descriptors_revents(playback->handle, ufds, count, &revents);
    if (revents & POLLERR)
        alsa_resource_recover(playback->handle);
    if (revents & POLLOUT)
        playback_transfer(playback);
}

void *capture_init() {
    struct capture *capture;
    unsigned int chn;
    unsigned int err, tmp, dir;

    capture = (struct capture*)malloc(sizeof(struct capture));
    if (capture == NULL) {
        //fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }

    if (err = snd_pcm_open(&capture->handle, device,
            SND_PCM_STREAM_CAPTURE, 0) < 0) {
        //fprintf(stderr, "Can't open '%s' PCM device. %s\n",
        //    device, snd_strerror(err));
        return NULL;
    }

    configure_alsa_pcm(capture->handle, capture_channels);

    capture->samples = (int16_t*)malloc((period_size * capture_channels * snd_pcm_format_physical_width(format)) / 8);
    if (capture->samples == NULL) {
        //fprintf(stderr, "1not enough memory\n");
        return NULL;
    }

    capture->areas = (snd_pcm_channel_area_t*)calloc(capture_channels, sizeof(snd_pcm_channel_area_t));
    if (capture->areas == NULL) {
        //fprintf(stderr, "2not enough memory\n");
        return NULL;
    }

    for (chn = 0; chn < capture_channels; ++chn) {
        capture->areas[chn].addr = capture->samples;
        capture->areas[chn].first = chn * snd_pcm_format_physical_width(format);
        capture->areas[chn].step = capture_channels * snd_pcm_format_physical_width(format);
    }

    if ((err = snd_pcm_start(capture->handle)) < 0) {
        //fprintf(stderr, "cannot start mic interface (%s)\n",
        //    snd_strerror(err));
        return NULL;
    }

    return capture;
}

void capture_close(void *data) {
    struct capture *capture = (struct capture *)data;
    free(capture->areas);
    free(capture->samples);
    snd_pcm_close(capture->handle);
    free(capture);
}

int capture_descriptors_count(void *data) {
    struct capture *capture = (struct capture *)data;
    return snd_pcm_poll_descriptors_count(capture->handle);
}

int capture_descriptors(void *data, struct pollfd *ufds, int count) {
    struct capture *capture = (struct capture *)data;
    int err;
    if ((err = snd_pcm_poll_descriptors(capture->handle,
            ufds, count)) < 0) {
        //printf("Unable to get capture descriptors: %s\n", snd_strerror(err));
        return err;
    }
    //printf("capture descriptors\n");
    return count;
}

void capture_handler(void *data, struct pollfd *ufds, int count) {
    struct capture *capture = (struct capture *)data;
    unsigned short revents;
    snd_pcm_poll_descriptors_revents(capture->handle, ufds, count, &revents);
    if (revents & POLLERR)
        alsa_resource_recover(capture->handle);
    if (revents & POLLIN)
        capture_transfer(capture);
}


struct resource_ops playback_ops = {
    playback_init,
    playback_close,
    playback_descriptors_count,
    playback_descriptors,
    playback_handler,
};

struct resource_ops capture_ops = {
    capture_init,
    capture_close,
    capture_descriptors_count,
    capture_descriptors,
    capture_handler,
};

