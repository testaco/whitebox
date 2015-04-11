#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

#include "dsp.h"

static char *device = "default";
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;
static unsigned int rate = 48000;
static unsigned int playback_channels = 2;
static unsigned int capture_channels = 1;
static unsigned int buffer_time = 500000;
static unsigned int period_time = 100000;
static uint32_t fcw1, fcw2;
static uint32_t phase1, phase2;
static int resample = 0;
static int verbose = 0;

static unsigned int tone1 = 700;
static unsigned int tone2 = 1900;

static int playback_enabled = 1;
static int capture_enabled = 1;

static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;

#define AUDIO_RING_SIZE 4096
static int16_t audio_ring[AUDIO_RING_SIZE];
static int audio_ring_head = 0;
static int audio_ring_tail = 0;

struct playback {
    snd_pcm_t *handle;
    int16_t *samples;
    snd_pcm_channel_area_t *areas;
};

void *playback_init() {
    struct playback *playback;
    unsigned int chn;
    unsigned int err, tmp, dir;

    playback = malloc(sizeof(struct playback));
    if (playback == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }

    if (err = snd_pcm_open(&playback->handle, device,
            SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Can't open '%s' PCM device. %s\n",
            device, snd_strerror(err));
        return NULL;
    }

    configure_alsa_pcm(playback->handle, playback_channels);

    playback->samples = malloc((period_size * playback_channels * snd_pcm_format_physical_width(format)) / 8);
    if (playback->samples == NULL) {
        fprintf(stderr, "1not enough memory\n");
        return NULL;
    }

    playback->areas = calloc(playback_channels, sizeof(snd_pcm_channel_area_t));
    if (playback->areas == NULL) {
        fprintf(stderr, "2not enough memory\n");
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
        printf("Unable to get playback descriptors: %s\n", snd_strerror(err));
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

struct capture {
    snd_pcm_t *handle;
    int16_t *samples;
    snd_pcm_channel_area_t *areas;
};

void *capture_init() {
    struct capture *capture;
    unsigned int chn;
    unsigned int err, tmp, dir;

    capture = malloc(sizeof(struct capture));
    if (capture == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }

    if (err = snd_pcm_open(&capture->handle, device,
            SND_PCM_STREAM_CAPTURE, 0) < 0) {
        fprintf(stderr, "Can't open '%s' PCM device. %s\n",
            device, snd_strerror(err));
        return NULL;
    }

    configure_alsa_pcm(capture->handle, capture_channels);

    capture->samples = malloc((period_size * capture_channels * snd_pcm_format_physical_width(format)) / 8);
    if (capture->samples == NULL) {
        fprintf(stderr, "1not enough memory\n");
        return NULL;
    }

    capture->areas = calloc(capture_channels, sizeof(snd_pcm_channel_area_t));
    if (capture->areas == NULL) {
        fprintf(stderr, "2not enough memory\n");
        return NULL;
    }

    for (chn = 0; chn < capture_channels; ++chn) {
        capture->areas[chn].addr = capture->samples;
        capture->areas[chn].first = chn * snd_pcm_format_physical_width(format);
        capture->areas[chn].step = capture_channels * snd_pcm_format_physical_width(format);
    }

    if ((err = snd_pcm_start(capture->handle)) < 0) {
        fprintf(stderr, "cannot start mic interface (%s)\n",
            snd_strerror(err));
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
        printf("Unable to get capture descriptors: %s\n", snd_strerror(err));
        return err;
    }
    printf("capture descriptors\n");
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

typedef int16_t (*source_next)();

static source_next source;

int16_t mute_source(void) {
    return 0;
}

int16_t tone_source(void) {
    static int16_t d = 0;
    int16_t i, q;
    sincos16(fcw1, &phase1, &i, &q);
    // Apply a CW shaping mask
    if (d < (1 << 14) - 1)
        d += 1;
    return (int16_t)(((int32_t)d * (int32_t)i) >> 15);
}

int16_t tone2_source(void) {
    int16_t i1, q1, i2, q2;
    sincos16(fcw1, &phase1, &i1, &q1);
    sincos16(fcw2, &phase2, &i2, &q2);
    return (i1 / 2 + i2 / 2);
}

int16_t awgn_source(void) {
    int16_t s;
    awgn(&s);
    return s;
}

// Read data from the microphone source, which is stored in the audio
// ring buffer.
int16_t mic_source(void) {
    int16_t sample;
    int end = AUDIO_RING_SIZE - audio_ring_tail;
    int n = (end + audio_ring_head) & (AUDIO_RING_SIZE - 1);
    int data = n < end ? n : end;
    if (data <= 0) {
        sample = 0;
    } else {
        sample = audio_ring[audio_ring_tail];
        audio_ring_tail = (audio_ring_tail + 1) & (AUDIO_RING_SIZE - 2);
    }
    return sample;
}

int16_t modem_source(void) {
    // TODO
    return 0;
}

struct sources_list {
    char *name;
    source_next next;
};

struct sources_list sources_list[] = {
    { "mute", mute_source },
    { "tone", tone_source },
    { "2tone", tone2_source },
    { "noise", awgn_source },
    { "mic", mic_source },
    { "modem", modem_source },
    { NULL, NULL },
};

int xrun_recovery(snd_pcm_t *handle, int err) {
    //fprintf(stdout, "Stream recovery\n");
    if (err == -EPIPE) {
        err = snd_pcm_prepare(handle);
        if (err < 0)
            fprintf(stderr, "Cant recover from underrun: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                fprintf(stderr, "Cant recover from underrun: %s\n", snd_strerror(err));
        return 0;
        }
    }
    return err;
}

typedef void (*sink_cb)(int16_t sample);

static sink_cb sink;

// Write data into the audio ring buffer, to be played on the speakers.
void speaker_sink(int16_t sample) {
    int end = AUDIO_RING_SIZE - 1 - audio_ring_head;
    int n = (end + audio_ring_tail) & (AUDIO_RING_SIZE - 1);
    int space = n <= end ? n : end + 1;
    if (space <= 0) {
        // Audio ring just drops samples; let's catch up!
        return;
    } else {
        audio_ring[audio_ring_head] = sample;
        audio_ring_head = (audio_ring_head + 1) & (AUDIO_RING_SIZE - 1);
    }
}

void null_sink(int16_t sample) {
    // Drop all of the samples.
    return;
}

void modem_sink(int16_t sample) {
    // TODO: do the bit manipulations to set up AFSK.
    // TODO: enqueue the samples; we can't drop these, so return an error.
    // TODO: we can't return an error, yet.
}

struct sinks_list {
    char *name;
    sink_cb next;
};

struct sinks_list sinks_list[] = {
    { "null", null_sink },
    { "speaker", speaker_sink },
    { "modem", modem_sink },
    { NULL, NULL },
};

int alsa_resource_recover(
        snd_pcm_t *handle) {
    int err;
    if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ||
        snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED) {
            err = snd_pcm_state(handle) == SND_PCM_STATE_XRUN ?
                -EPIPE : -ESTRPIPE;
        if (xrun_recovery(handle, err) < 0) {
            fprintf(stderr, "Write error: %s\n", snd_strerror(err));
            exit(-1);
        }
    }
}

int playback_transfer(
        struct playback *playback) {
    static init = 1;
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
            printf("boo\n");
            exit(-1);
        }
        samples[chn] = (((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
        if ((areas[chn].step % 16) != 0) {
            printf("boo2\n");
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
                printf("Write error: %s\n", snd_strerror(err));
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
    static init = 1;
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
            printf("boo\n");
            exit(-1);
        }
        samples[chn] = (((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
        if ((areas[chn].step % 16) != 0) {
            printf("boo2\n");
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
                printf("Read error: %s\n", snd_strerror(err));
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

struct resource_ops {
    // Create and initialize the resource in memory.
    void *(*init)();

    // Close and free the resource.
    void (*close)(void *);

    // How many descriptors this resource needs poll to track.
    int (*descriptors_count)(void *);

    // Fill in the poll descriptor list.
    int (*descriptors)(void *, struct pollfd *, int);

    // Called every main loop iteration to notify the resource of poll updates.
    void (*handler)(void *, struct pollfd *, int);
};

// The object tracked by the main loop.
struct resource {
    char name[256];
    struct resource_ops *ops;
    void *data;

    int descriptor_offset;
    int descriptor_count;
};

struct resource_ops playback_ops = {
    .init = playback_init,
    .close = playback_close,
    .descriptors_count = playback_descriptors_count,
    .descriptors = playback_descriptors,
    .handler = playback_handler,
};

struct resource_ops capture_ops = {
    .init = capture_init,
    .close = capture_close,
    .descriptors_count = capture_descriptors_count,
    .descriptors = capture_descriptors,
    .handler = capture_handler,
};

int resource_init(struct resource *r) {
    r->data = r->ops->init();
    if (r->data != NULL)
        return 0;
    return -1;
}

void resource_close(struct resource *r) {
    r->ops->close(r->data);
}

int resource_descriptors_count(struct resource *r) {
    return r->ops->descriptors_count(r->data);
}

int resource_descriptors(struct resource *r,
        struct pollfd *ufds, int count) {
    return r->ops->descriptors(r->data, ufds, count);
}

void resource_handler(struct resource *r,
        struct pollfd *ufds, int count) {
    r->ops->handler(r->data, ufds, count);
}

int resource_setup(struct resource *r, char *name, struct resource_ops *ops) {
    strncpy(r->name, name, 255);
    r->ops = ops;
    return resource_init(r);
}

int transfer_loop(struct resource *resources, int resource_count) {
    struct pollfd *ufds;
    signed short *ptr;
    int err, cptr;
    int i;
    int count = 0;

    // Get how many polling descriptors there are
    for (i = 0; i < resource_count; ++i) {
        struct resource *r = &resources[i];
        r->descriptor_offset = count;
        r->descriptor_count = resource_descriptors_count(r);
        if (r->descriptor_count <= 0) {
            fprintf(stderr, "Invalid poll descriptors count\n");
            return count;
        }
        count += r->descriptor_count;
    }

    if (count <= 0) {
        fprintf(stderr, "No resources to work with\n");
        return -1;
    }

    // Fill in the polling descriptors
    ufds = malloc(sizeof(struct pollfd) * count);
    if (ufds == NULL) {
        printf("Not enough memory\n");
        return -ENOMEM;
    }

    for (i = 0; i < resource_count; ++i) {
        struct resource *r = &resources[i];
        if ((err = resource_descriptors(r, &ufds[r->descriptor_offset],
                r->descriptor_count)) < 0)
            return err;
    }

    // The main loop
    while (1) {
        // First, poll.
        err = poll(ufds, count, -1);

        if (err < 0)
            exit(-1);
        
        // Then notify each resource that loop iteration has complete.
        for (i = 0; i < resource_count; ++i) {
            struct resource *r = &resources[i];
            resource_handler(r, &ufds[r->descriptor_offset],
                    r->descriptor_count);
        }
    }

    free(ufds);
    return 0;
}

// Utility function for setting up an alsa device
int configure_alsa_pcm(snd_pcm_t *handle, unsigned int nch) {
    int pcm, tmp, dir;
    snd_pcm_hw_params_t *params;
    
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);

    if (pcm = snd_pcm_hw_params_set_access(handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        fprintf(stderr, "Can't set interleaved mode. %s\n",
            snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params_set_format(handle, params,
            format) < 0) {
        fprintf(stderr, "Can't set format. %s\n",
            snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params_set_channels(handle, params,
            nch) < 0) {
        fprintf(stderr, "Can't set channels. %s\n",
            snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params_set_rate_near(handle, params,
            &rate, 0) < 0) {
        fprintf(stderr, "Can't set rate. %s\n",
            snd_strerror(pcm));
        return -1;
    }

    if (pcm = snd_pcm_hw_params(handle, params) < 0) {
        fprintf(stderr, "Can't set hw params. %s\n",
            snd_strerror(pcm));
        return -1;
    }

    fprintf(stdout, "PCM name: '%s'\n", snd_pcm_name(handle));
    fprintf(stdout, "PCM state: '%s'\n", snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_channels(params, &tmp);
    fprintf(stdout, "channels: %d\n", tmp);

    snd_pcm_hw_params_get_rate(params, &tmp, 0);
    fprintf(stdout, "rate: %d\n", rate);

    fprintf(stdout, "format width: %d\n", snd_pcm_format_physical_width(format));

    if (pcm = snd_pcm_hw_params_get_period_size(params, &period_size, &dir) < 0)
        fprintf(stderr, "Can't get period size. %s\n",
            snd_strerror(pcm));

    return 0;
}

int parse_args(int argc, char **argv) {
    int i, source_found = 0, sink_found = 0;

    struct option long_option[] = {
        { "help", 0, NULL, 'h' },
        { "device", 1, NULL, 'D' },
        { "rate", 1, NULL, 'r' },
        { "channels", 1, NULL, 'c' },
        { "tone", 1, NULL, 'f' },
        { "buffer", 1, NULL, 'b' },
        { "period", 1, NULL, 'p' },
        { "mode", 1, NULL, 'm' },
        { "format", 1, NULL, 'o' },
        { "verbose", 1, NULL, 'v' },
        { "source", 1, NULL, 'u' },
        { "sink", 1, NULL, 'i' },
        { NULL, 0, NULL, 0 },
    };

    while (1) {
        int c;
        if ((c = getopt_long(argc, argv, "hD:r:c:f:b:p:m:o:vu:i:ne", long_option, NULL)) < 0)
            break;
        switch (c) {
            case 'h':
                // TODO
                break;
            case 'D': 
                device = strdup(optarg);
                break;
            case 'r':
                rate = atoi(optarg);
                break;
            case 'c':
                playback_channels = atoi(optarg);
                break;
            case 'f':
                // TODO
                break;
            case 'b':
                // TODO
                break;
            case 'p':
                // TODO
                break;
            case 'm':
                // TODO
                break;
            case 'o':
                // TODO
                break;
            case 'v':
                verbose = 1;
                break;
            case 'u':
                // select source
                for (i = 0; sources_list[i].next; ++i) {
                    if (strcmp(sources_list[i].name, optarg) == 0) {
                        source = sources_list[i].next;
                        source_found = 1;
                    }
                }
                if (!source_found) { // Not a valid option
                    fprintf(stderr, "Invalid source: %s\n", optarg);
                    return -1;
                }
                break;
            case 'i':
                // select sink
                for (i = 0; sinks_list[i].next; ++i) {
                    if (strcmp(sinks_list[i].name, optarg) == 0) {
                        sink = sinks_list[i].next;
                        sink_found = 1;
                    }
                }
                if (!sink_found) { // Not a valid option
                    fprintf(stderr, "Invalid sink: %s\n", optarg);
                    return -1;
                }
                break;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    struct resource resources[16];
    int resource_count = 0;
    int i;

    // Defaults
    source = &mic_source;
    sink = &speaker_sink;

    // Pick up configuration from command line
    if (parse_args(argc, argv) < 0)
        return -1;

    // Recompute DDS words based on final sample rate
    fcw1 = freq_to_fcw(tone1, rate);
    fcw2 = freq_to_fcw(tone2, rate);

    // Initialize DSP library
    dsp_init();

    // Create the audio resources
    if (playback_enabled) {
        if (resource_setup(&resources[resource_count++], "playback",
                &playback_ops) < 0) {
            fprintf(stderr, "Can't initialize playback\n");
            return -1;
        }
    }

    if (capture_enabled) {
        if (resource_setup(&resources[resource_count++], "capture",
                &capture_ops) < 0) {
            fprintf(stderr, "Can't initialize capture\n");
            return -1;
        }
    }

    // Create the modem resources

    // Main transfer loop
    transfer_loop(resources, resource_count);

    // Close everything
    for (i = 0; i < resource_count; ++i) {
        resource_close(&resources[i]);
    }

    return 0;
}
