#include <iostream>
#include <cstdlib>
#include <list>
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
//#include "modem.h"

static uint32_t fcw1, fcw2;
static uint32_t phase1, phase2;
static int resample = 0;
static int verbose = 0;
static int port = 80;

static unsigned int tone1 = 400;
static unsigned int tone2 = 1900;

static int modem_enabled = 0;
static int server_enabled = 1;
static int file_enabled = 0;
static int playback_enabled = 0;
static int capture_enabled = 0;

#if 0
#define AUDIO_RING_SIZE 4096
static int16_t audio_ring[AUDIO_RING_SIZE];
static int audio_ring_head = 0;
static int audio_ring_tail = 0;
#endif

static char* device;
static int rate;
static int playback_channels;

static const unsigned int max_fd_count = 1024;
static unsigned int fd_count = 0;
pollfd fds[max_fd_count] = { };
void * fds_data[max_fd_count] = { };
poll_handler fds_handler[max_fd_count];

void
poll_init()
{
    fd_count = 0;
    for (int i = 0; i < max_fd_count; ++i) {
        memset(&fds[i], 0, sizeof(*fds));
        fds_handler[i] = 0;
        fds_data[i] = 0;
    }
}

void
poll_debug_fds()
{
    for ( unsigned int i = 0; i < fd_count; i++ ) {
        std::cerr << "\tfd=" << fds[i].fd <<
        "events=" << fds[i].events << "revents=" << fds[i].revents << std::endl;
    }
}

void
poll_start_fd(int fd, int events, poll_handler handler, void *data)
{
    if ( fd_count >= max_fd_count ) {
        std::cerr << "fd_count too large in poll_start_fd().";
        return;
    }
    pollfd * f = &fds[fd_count];
    f->fd = fd;
    f->events = events;
    f->revents = 0;
    fds_handler[fd_count] = handler;
    fds_data[fd_count] = data;
    fd_count++;

    //poll_debug_fds();
}

void
poll_change_fd(int fd, int events)
{
    for ( unsigned int i = 0; i < fd_count; i++ ) {
        if ( fds[i].fd == fd ) {
            fds[i].events = events;
            //poll_debug_fds();
            return;
        }
    }
    std::cerr << "fd not found in poll_change_fd()." << std::endl;
}

void
poll_end_fd(int fd)
{
    for ( unsigned int i = 0; i < fd_count; i++ ) {
        if ( fds[i].fd == fd ) {
            if ( --fd_count > i ) {
                fds[i] = fds[fd_count];
                fds_handler[i] = fds_handler[fd_count];
                fds_data[i] = fds_data[fd_count];
            }
            memset(&fds[fd_count], 0, sizeof(*fds));
            memset(&fds_handler[fd_count], 0, sizeof(*fds_handler));
            memset(&fds_data[fd_count], 0, sizeof(*fds_data));
            //poll_debug_fds();
            return;
        }
    }
    //poll_debug_fds();
    std::cerr << "fd " << fd << " not found in poll_end_fd(), fd_count is " << fd_count << std::endl;
}

std::list<routine_handler *> routines;

extern void poll_start_routine(routine_handler * handler) {
    routines.push_back(handler);
    std::cerr << "Poll: start routine (" << routines.size() << ")" << std::endl;
}

extern void poll_end_routine(routine_handler * handler) {
    routines.remove(handler);
    std::cerr << "Poll: end routine (" << routines.size() << ")" << std::endl;
}

void poll_run_routines() {
    std::list<routine_handler *>::const_iterator i;
    for (i = routines.begin(); i != routines.end(); ++i) {
        (*i)->callback();
    }
}

int run_forever() {
    int status;

    if (fd_count <= 0) {
        std::cerr << "no fds to watch" << std::endl;
        return -1;
    }

    // The main loop
    while (1) {
        // Step 1, Calculate timeout.
        int timeout = -1;

        // Step 2, Wait for IO.
        status = poll(fds, fd_count, timeout);

        // Step 3, Process IO events.
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
            if ( fds[i].revents && fds_handler[i] ) {
              fds_handler[i](&fds[i], fds_data[i]);
              serviced++;
            } 
          }
          if ( serviced != status ) {
            std::cerr << "Didn't find file descriptor to service." << std::endl;
            fprintf(stderr, "fd_count is %d\n", fd_count);
            fprintf(stderr, "status count is %d\n", status);
            fprintf(stderr, "serviced count is %d\n", serviced);
            //poll_debug_fds();
          }
        }

        // Step 4, Process timer events.

        // Step 5, Run routines.
        poll_run_routines();
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
        { "mode", 1, NULL, 'm' },
        { "format", 1, NULL, 'o' },
        { "verbose", 1, NULL, 'v' },
        { "source", 1, NULL, 'u' },
        { "sink", 1, NULL, 'i' },
        { "port", 1, NULL, 'p' },
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
                port = atoi(optarg);
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
                #if 0
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
                #endif
                break;
            case 'i':
                // select sink
                #if 0
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
                #endif
                break;
        }
    }
    #if 0
    if (!device)
        device = strdup("default");
    #endif
    return 0;
}

int main(int argc, char **argv) {
    poll_init();

    // Defaults
    //source = &mic_source;
    //sink = &speaker_sink;

    // Pick up configuration from command line
    if (parse_args(argc, argv) < 0)
        return -1;

    // Recompute DDS words based on final sample rate
    fcw1 = freq_to_fcw(tone1, rate);
    fcw2 = freq_to_fcw(tone2, rate);

    // Initialize DSP library
    dsp_init();

#if 0
    if (modem_enabled) {
        if (resource_setup(&resources[resource_count++], "modem",
                &modem_ops) < 0) {
            fprintf(stderr, "Can't initialize modem\n");
            return -1;
        }
    }
#endif

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
        server_start(0, port, false);
    }

#if 0
    // Create the file
    if (file_enabled) {
        if (resource_setup(&resources[resource_count++], "file",
                &file_source_ops) < 0) {
           fprintf(stderr, "Can't open file source\n");
           return -1;
        }
    }
#endif

    // Main transfer loop
    run_forever();

#if 0
    // Close everything
    for (i = 0; i < resource_count; ++i) {
        resource_close(&resources[i]);
    }
#endif

    return 0;
}
