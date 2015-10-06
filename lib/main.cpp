#include <iostream>
#include <cstdlib>
#if ALSA_FOUND
#include <alsa/asoundlib.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>

#include "dsp.h"

#include "cJSON.h"
#include "radio.h"
//#include "soundcard.h"
#include "modem.h"

static uint32_t fcw1, fcw2;
static uint32_t phase1, phase2;
static int resample = 0;
static int verbose = 0;

static unsigned int tone1 = 400;
static unsigned int tone2 = 1900;

static int modem_enabled = 0;
static int server_enabled = 1;
static int file_enabled = 0;
static int playback_enabled = 0;
static int capture_enabled = 0;

#define AUDIO_RING_SIZE 4096
static int16_t audio_ring[AUDIO_RING_SIZE];
static int audio_ring_head = 0;
static int audio_ring_tail = 0;

static char* device;
static int rate;
static int playback_channels;

int16_t mute_source(void) {
    return 0;
}

int16_t tone_source(void) {
    static int16_t d = 0;
    int16_t i, q;
    sincos16(fcw1, &phase1, &i, &q);
    // Apply a CW shaping mask
    //if (d < (1 << 14) - 1)
    //    d += 1;
    //return (int16_t)(((int32_t)d * (int32_t)i) >> 15);
    return i >> 2; //QUAD_PACK(i >> 1, q >> 1);
}

int16_t tone2_source(void) {
    int16_t i1, q1, i2, q2;
    sincos16(fcw1, &phase1, &i1, &q1);
    sincos16(fcw2, &phase2, &i2, &q2);
    //return (i1 / 2 + i2 / 2);
    int16_t i3 = i1 >> 2 + i2 >> 2;
    //int16_t q3 = q1 >> 2 + q2 >> 2;
    return i3 >> 1;
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
    const char *name;
    source_next next;
};

static const struct sources_list sources_list[] = {
    { "mute", mute_source },
    { "tone", tone_source },
    { "2tone", tone2_source },
    { "noise", awgn_source },
    { "mic", mic_source },
    { "modem", modem_source },
    { NULL, NULL },
};

int sink_space() {
    int end = AUDIO_RING_SIZE - 1 - audio_ring_head;
    int n = (end + audio_ring_tail) & (AUDIO_RING_SIZE - 1);
    int space = n <= end ? n : end + 1;
    return space;
}

// Write data into the audio ring buffer, to be played on the speakers.
void speaker_sink(int16_t sample) {
    int space = sink_space();
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
    const char *name;
    sink_cb next;
};

static const struct sinks_list sinks_list[] = {
    { "null", null_sink },
    { "speaker", speaker_sink },
    { "modem", modem_sink },
    { NULL, NULL },
};

struct server {
    int placeholder;
};

void *server_init() {
    struct server *server;
    server = (struct server*)malloc(sizeof(struct server));
    if (server == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }
    if (!server_start(0, 80, false)) {
        fprintf(stderr, "Can't start the server.\n");
        free(server);
        return NULL;
    }
    return server;
}

void server_close(void *data) {
    struct server *server = (struct server *)data;
    //server_end();
    free(server);
}

int server_descriptors_count(void *data) {
    struct server *server = (struct server *)data;
    return 0;
}

int server_descriptors(void *data, struct pollfd *ufds, int count) {
    struct server *server = (struct server *)data;
    return 0;
}

void server_handler(void *data, struct pollfd *ufds, int count) {
    
}

struct resource_ops server_ops = {
    server_init,
    server_close,
    server_descriptors_count,
    server_descriptors,
    server_handler,
};





struct file_source {
    int fd;
};

void *file_source_init() {
    struct file_source *file_source;
    file_source = (struct file_source*)malloc(sizeof(struct file_source));
    if (file_source == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return NULL;
    }
    file_source->fd = open("/mnt/whitebox/gnuradio/audio_50k.samples", O_RDONLY | O_NONBLOCK);
    if (file_source->fd < 0) {
        perror("file_source open");
        free(file_source);
        return NULL;
    }
    poll_start_fd(0, file_source->fd, POLLIN, FILE_SOURCE_FD);
    return file_source;
}

void file_source_close(void *data) {
    struct file_source *file_source = (struct file_source *)data;
    poll_end_fd(file_source->fd);
    close(file_source->fd);
    free(file_source);
}

int file_source_descriptors_count(void *data) {
    struct file_source *file_source = (struct file_source *)data;
    return 1;
}

int file_source_descriptors(void *data, struct pollfd *ufds, int count) {
    struct file_source *file_source = (struct file_source *)data;
    return 0;
}

void file_source_handler(void *data, struct pollfd *ufds, int count) {
    
}

void file_source_to_sink(int fd) {
#if 0
    int16_t audio_data[1024];
    if (sink_space() > 0) {
        int count = read(fd, (void*)audio_data, 1024 * 2);
        if (count < 0) {
            perror("read");
            exit(-1);
        } else if (count == 0) {
            std::cerr << "reset file" << std::endl;
            lseek(fd, 0, SEEK_SET);
        } else {
            for (int i = 0; i < count / 2; ++i) {
                //std::cerr << ' ' << audio_data[i];
                sink(audio_data[i]);
            }
            //std::cerr << std::endl;
        }
    }
#endif
}

struct resource_ops file_source_ops = {
    file_source_init,
    file_source_close,
    file_source_descriptors_count,
    file_source_descriptors,
    file_source_handler,
};





static const unsigned int max_fd_count = 1024;
static unsigned int fd_count = 0;
pollfd fds[max_fd_count] = { };
libwebsocket_context * websocket_contexts[max_fd_count] = { };
static int whitebox_fd = -1;
static int file_source_fd = -1;

void
poll_initialize_contexts()
{
    for (int i = 0; i < max_fd_count; ++i) {
        websocket_contexts[i] = 0;
    }
}

void
poll_debug_fds()
{
    for ( unsigned int i = 0; i < fd_count; i++ ) {
      fprintf(stderr, "\tfd=%d, events=%d, revents=%d, type=%s\n",
        fds[i].fd, fds[i].events, fds[i].revents,
        websocket_contexts[i] ? "websocket" : "whitebox" );
    }
}

void
poll_start_fd(libwebsocket_context * context, int fd, int events, int type)
{
  fprintf(stderr, "poll_start_fd %d websocket?%s\n", fd, context ? "yes" : "no");
   if ( fd_count >= max_fd_count ) {
     std::cerr << "fd_count too large in poll_start_fd().";
     return;
   }
   pollfd * f = &fds[fd_count];
   f->fd = fd;
   f->events = events;
   f->revents = 0;
   websocket_contexts[fd_count] = context;
   fd_count++;
   poll_debug_fds();
   if (type == WHITEBOX_FD) // a little hacky.
     whitebox_fd = fd;
   if (type == FILE_SOURCE_FD)
     file_source_fd = fd;
}

void
poll_change_fd(int fd, int events)
{
  fprintf(stderr, "poll_change_fd %d\n", fd);
  for ( unsigned int i = 0; i < fd_count; i++ ) {
    if ( fds[i].fd == fd ) {
      fds[i].events = events;
      poll_debug_fds();
      return;
    }
  }
  std::cerr << "fd not found in poll_change_fd()." << std::endl;
  exit(-1);
}

void
poll_end_fd(int fd)
{
  for ( unsigned int i = 0; i < fd_count; i++ ) {
    if ( fds[i].fd == fd ) {
      if ( --fd_count > i ) {
        fds[i] = fds[fd_count];
        websocket_contexts[i] = websocket_contexts[fd_count];
      }
      memset(&fds[fd_count], 0, sizeof(*fds));
      memset(&websocket_contexts[fd_count], 0, sizeof(*websocket_contexts));
      poll_debug_fds();
      return;
    }
  }
  poll_debug_fds();
  std::cerr << "fd " << fd << " not found in poll_end_fd(), fd_count is " << fd_count << std::endl;
}

int transfer_loop(struct resource *resources, int resource_count) {
    signed short *ptr;
    int status, cptr;

    int i;

    if (fd_count <= 0) {
        std::cerr << "no fds to watch" << std::endl;
        return -1;
    }

    // The main loop
    while (1) {
        // First, poll.
        status = poll(fds, fd_count, -1);

        if (status < 0) {
            perror("poll");
            exit(-1);
        } else if (status == 0) {
            //fprintf(stderr, "Nothing to poll\n");
            //poll_debug_fds();
        }
        else if (status > 0) {
          int serviced = 0;
          for ( unsigned int i = 0; serviced < status && i < fd_count; i++ ) {
            if ( fds[i].revents && websocket_contexts[i] ) {
              server_service_fd(websocket_contexts[i], &fds[i]);
              serviced++;
            } else if ( fds[i].revents && fds[i].fd == whitebox_fd ) {  // the whitebox
                if (fds[i].revents & POLLOUT) {
                    //fprintf(stderr, "pollout\n");
                    modem_write();
                }
                if (fds[i].revents & POLLIN) {
                    //fprintf(stderr, "pollin\n");
                    //modem_read();
                }
                if (fds[i].revents & POLLERR) {
                    //fprintf(stderr, "pollerr\n");
                    modem_recover();
                }
                serviced++;
            } else if (fds[i].revents && fds[i].fd == file_source_fd) {
                file_source_to_sink(fds[i].fd);
                serviced++;
            }
          }
          if ( serviced != status ) {
            std::cerr << "Didn't find file descriptor to service." << std::endl;
            fprintf(stderr, "fd_count is %d\n", fd_count);
            fprintf(stderr, "status count is %d\n", status);
            fprintf(stderr, "serviced count is %d\n", serviced);
            poll_debug_fds();
          }
        }
    }

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
    if (!device)
        device = strdup("default");
    return 0;
}

int main(int argc, char **argv) {
    struct resource resources[16];
    int resource_count = 0;
    int i;

    poll_initialize_contexts();

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

    if (modem_enabled) {
        if (resource_setup(&resources[resource_count++], "modem",
                &modem_ops) < 0) {
            fprintf(stderr, "Can't initialize modem\n");
            return -1;
        }
    }

#if ALSA_FOUND
    // Create the speaker playback resource
    if (playback_enabled) {
        if (resource_setup(&resources[resource_count++], "playback",
                &playback_ops) < 0) {
            fprintf(stderr, "Can't initialize playback\n");
            return -1;
        }
    }

    // Create the microphone capture source
    if (capture_enabled) {
        if (resource_setup(&resources[resource_count++], "capture",
                &capture_ops) < 0) {
            fprintf(stderr, "Can't initialize capture\n");
            return -1;
        }
    }
#endif

    // Create the websocket server
    if (server_enabled) {
        if (resource_setup(&resources[resource_count++], "server",
                &server_ops) < 0) {
            fprintf(stderr, "Can't initialize server\n");
            return -1;
        }
    }

    // Create the file
    if (file_enabled) {
        if (resource_setup(&resources[resource_count++], "file",
                &file_source_ops) < 0) {
           fprintf(stderr, "Can't open file source\n");
           return -1;
        }
    }

    // Main transfer loop
    transfer_loop(resources, resource_count);

    // Close everything
    for (i = 0; i < resource_count; ++i) {
        resource_close(&resources[i]);
    }

    return 0;
}
