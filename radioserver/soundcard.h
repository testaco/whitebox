#ifndef __SOUNDCARD_H__
#define __SOUNDCARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "resources.h"

extern char *device;
extern unsigned int rate;
extern unsigned int playback_channels;
extern unsigned int capture_channels;
extern unsigned int buffer_time;
extern unsigned int period_time;

struct playback {
    snd_pcm_t *handle;
    int16_t *samples;
    snd_pcm_channel_area_t *areas;
};

struct capture {
    snd_pcm_t *handle;
    int16_t *samples;
    snd_pcm_channel_area_t *areas;
};

void *playback_init();
void playback_close(void *);
int playback_descriptors_count(void *);
int playback_descriptors(void *, struct pollfd *, int);
void playback_handler(void *, struct pollfd *, int);

extern struct resource_ops playback_ops;

void *capture_init();
void capture_close(void *);
int capture_descriptors_count(void *);
int capture_descriptors(void *, struct pollfd *, int);
void capture_handler(void *, struct pollfd *, int);

extern struct resource_ops capture_ops;

#ifdef __cplusplus
}
#endif

#endif /* __SOUNDCARD_H__ */
