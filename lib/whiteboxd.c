#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <poll.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "whitebox.h"

#define PORT 11287
#define FRAME_SIZE 512

#define NUM_WEAVER_TAPS 109
int16_t weaver_fir_taps[] = { 0, 0, 0, 0, 1, 1, 0, -1, -2, -1, 0, 1, 2, 2, 0, -2, -3, -2, 0, 3, 5, 3, 0, -4, -6, -5, 0, 5, 8, 6, 0, -7, -11, -8, 0, 9, 14, 11, 0, -13, -20, -15, 0, 19, 30, 23, 0, -31, -51, -44, 0, 74, 158, 224, 249, 224, 158, 74, 0, -44, -51, -31, 0, 23, 30, 19, 0, -15, -20, -13, 0, 11, 14, 9, 0, -8, -11, -7, 0, 6, 8, 5, 0, -5, -6, -4, 0, 3, 5, 3, 0, -2, -3, -2, 0, 2, 2, 1, 0, -1, -2, -1, 0, 1, 1, 0, 0, 0, 0 };

float diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1e9+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return (temp.tv_sec * 1e9 + temp.tv_nsec) / 1e9 * 1e3;
}

enum whitebox_mode {
    WBM_INIT        = 0x00000000,
    WBM_IQ_SOCKET   = 0x00000001,
    WBM_IQ_FILE     = 0x00000002,
    WBM_IQ_TONE     = 0x00000004,
    WBM_IQ_TEST     = 0x00000010,
    WBM_AUDIO_TONE  = 0x00000100,
};

#define WBM_DEFAULT   WBM_IQ_SOCKET
#define WBM_IQ_DIRECT (WBM_IQ_SOCKET | WBM_IQ_FILE)
#define WBM_IQ_MIXED  (WBM_AUDIO_TONE)

struct whitebox_config {
    enum whitebox_mode mode;

    int verbose_flag;
    int sample_rate;
    float carrier_freq;
    unsigned short port;
    float duration;
    
    // complex variable z
    char *z_filename;
    float z_tone;

    // real variable x
    float x_tone;
    int16_t x_offset;
};

struct whitebox_runtime {
    int fd;
    int i;
    
    int z_fd;
    int z_needs_poll;
    uint32_t z_fcw;
    uint32_t z_phase;

    uint32_t x_fcw;
    uint32_t x_phase;

    int16_t global_re;
    int16_t global_im;

    int listening_fd;
    struct sockaddr_in sock_me, sock_other;
    size_t slen;
};

void whitebox_config_init(struct whitebox_config *config)
{
    config->mode = WBM_INIT;

    config->verbose_flag = 0;
    config->sample_rate = 48e3;
    config->carrier_freq = 144.00e6;
    config->port = PORT;
    config->duration = 0.00;

    config->z_filename = NULL;
    config->z_tone = 6e3;

    config->x_tone = 0;
    config->x_offset = 0;
}

void whitebox_runtime_init(struct whitebox_runtime *rt)
{
    rt->fd = -1;
    rt->i = 0;
    rt->z_fd = -1;
    rt->z_needs_poll = 0;
    rt->z_fcw = 0;
    rt->z_phase = 0;
    rt->x_fcw = 0;
    rt->x_phase = 0;
    rt->global_re = -1;
    rt->global_im = -1;
    rt->listening_fd = -1;
    rt->slen = sizeof(rt->sock_other);
}

int whitebox_start(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    struct timespec res;
    dsp_init();
    whitebox_init(wb);
    if ((rt->fd = whitebox_open(wb, "/dev/whitebox", O_RDWR, config->sample_rate)) < 0) {
        perror("open");
        return -1;
    }
    if (whitebox_mmap(wb) < 0) {
        perror("mmap");
        return -1;
    }
    if (config->mode & WBM_IQ_SOCKET) {
        if ((rt->listening_fd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==-1) {
            perror("socket");
            return -1;
        }

        memset((char *) &rt->sock_me, 0, sizeof(rt->sock_me));
        rt->sock_me.sin_family = AF_INET;
        rt->sock_me.sin_port = htons(config->port);
        rt->sock_me.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(rt->listening_fd, (struct sockaddr*)&rt->sock_me, sizeof(rt->sock_me))==-1) {
            perror("bind");
            return -1;
        }
        if (listen(rt->listening_fd, 0) == -1) {
            perror("listen");
            return -1;
        }
        if ((rt->z_fd = accept(rt->listening_fd, (struct sockaddr*)&rt->sock_other, &rt->slen)) == -1) {
            perror("accept");
            return -1;
        }
        if(config->verbose_flag)
            printf("Accepted client\n");
    } else if (config->mode & WBM_IQ_FILE) {
        if ((rt->z_fd = open(config->z_filename, O_RDONLY | O_NONBLOCK)) < 0) {
            perror("open");
            return -1;
        }
    } else if (config->mode & WBM_IQ_TONE) {
        rt->z_fcw = freq_to_fcw(config->z_tone, config->sample_rate);
        rt->z_phase = 0;
    }
    
    if (config->mode & WBM_AUDIO_TONE) {
        rt->x_fcw = freq_to_fcw(config->x_tone, config->sample_rate);
        rt->x_phase = 0;
    }

    whitebox_fir_load_coeffs(wb, 0, NUM_WEAVER_TAPS, weaver_fir_taps);

    if (config->verbose_flag) {
        int16_t coeffs[WF_COEFF_COUNT];
        int i, n;
        clock_getres(CLOCK_MONOTONIC, &res);
        printf("Clock resolution: %f ms\n", (res.tv_sec * 1e9 + res.tv_nsec) / 1e6);
        printf("sincos LUT size: %d kB, %d bits\n", DDS_RAM_SIZE_BITS >> 13, DDS_RAM_SIZE_BITS);
        n = whitebox_fir_get_coeffs(wb, 0, WF_COEFF_COUNT, &coeffs);
        printf("FIR coefficients: [");
        for (i = 0; i < n-1; ++i)
            printf("%d, ", coeffs[i]);
        printf("%d]\n", coeffs[n-1]);
    }

    return 0;
}

int whitebox_run(whitebox_t *wb, struct whitebox_config *config,
        struct whitebox_runtime *rt)
{
    int total_samples = 0;
    struct timespec tl_start, tl_poll, tl_sink, tl_source, tl_mix, tl_finish;
    long in_available, out_available;
    struct pollfd fds[2];
    uint32_t *dest;
    uint32_t *z_dest, *z_dest_offset;
    uint32_t z_buf[FRAME_SIZE];

    z_dest = NULL;
    z_dest_offset = NULL;

    if (config->duration > 0.0)
        total_samples = config->duration * config->sample_rate;

    printf("WBMODE %08x %d", config->mode, total_samples);

    whitebox_tx(wb, config->carrier_freq);

    if (config->mode & WBM_IQ_MIXED) {
        whitebox_tx_flags_enable(wb, WS_FIREN);
    } else {
        whitebox_tx_flags_disable(wb, WS_FIREN);
    }

    whitebox_debug_to_file(wb, stdout);

    fds[0].fd = rt->z_fd;
    fds[0].events = POLLIN;

    while (((total_samples > 0) ? (rt->i < total_samples) : 1)) {
        clock_gettime(CLOCK_MONOTONIC, &tl_start);
        // step 1. poll audio source for reading and whitebox sink for writing
        if (rt->z_needs_poll && poll(fds, 1, 50) <= 0) {
            continue;
        }
        clock_gettime(CLOCK_MONOTONIC, &tl_poll);

        // step 2. get the destination address and available space in whitebox sink
        out_available = ioctl(rt->fd, W_MMAP_WRITE, &dest);
        if (out_available == 0) {
            //printf("q"); fflush(stdout);
            continue;
        }
        out_available = out_available < (FRAME_SIZE << 2) ? out_available : (FRAME_SIZE << 2);
        if (total_samples > 0 && (out_available >> 2) + rt->i > total_samples)
            out_available = (total_samples - rt->i) << 2;

        if (!(config->mode & WBM_IQ_MIXED))
            z_dest = dest;
        else
            z_dest = z_buf;

        clock_gettime(CLOCK_MONOTONIC, &tl_sink);

        // step 3. recv_from from the iq/z source
        in_available = 0;
        if (out_available > 0 && (!rt->z_needs_poll || (fds[0].revents & POLLIN))) {
            if (config->mode & WBM_IQ_SOCKET) {
                int recvd;
                z_dest_offset = z_dest;
                while (out_available > 0) {
                    if ((recvd = recvfrom(rt->z_fd, (void*)z_dest_offset, out_available, 0, (struct sockaddr*)&rt->sock_other, &rt->slen)) < 0) {
                        if (errno == EAGAIN) {
                            break;
                        }
                        perror("recvfrom()");
                        exit(1);
                    }
                    if (recvd == 0)
                        break;
                    if (recvd == 1 && *((char*)z_dest_offset) == '\0')
                        break;
                    if (recvd % 4 != 0)
                        printf("not word aligned\n");
                    //printf("z"); fflush(stdout);
                    in_available += recvd;
                    out_available -= recvd;
                    z_dest_offset += recvd >> 2;
                }
                if (recvd == 1 && *((char*)z_dest_offset) == '\0')
                    break;
                if (recvd == 0)
                    break;
            } else if (config->mode & WBM_IQ_FILE) {
                int recvd;
                z_dest_offset = z_dest;
                while (out_available > 0) {
                    if ((recvd = read(rt->z_fd, (void*)z_dest_offset, out_available)) < 0) {
                        if (errno == EAGAIN) {
                            printf("w");
                            break;
                        }
                        perror("read");
                        exit(1);
                    }
                    if (recvd % 4 != 0)
                        printf("not word aligned\n");
                    if (recvd == 0)
                        break;
                    in_available += recvd;
                    out_available -= recvd;
                    z_dest_offset += recvd >> 2;
                }
                if (recvd == 0)
                    break;
            } else if (config->mode & WBM_IQ_TONE) {
                int n;
                int16_t re, im;
                for (n = 0; n < out_available >> 2; ++n) {
                    QUAD_UNPACK(sincos16c(rt->z_fcw, &rt->z_phase), re, im);
                    *((uint32_t*)(z_dest + n)) = QUAD_PACK(re >> 1, im >> 1);
                }
                in_available = out_available;
            }
        }

        if (config->mode & WBM_IQ_TEST) {
            int n;
            int16_t re, im;
            for (n = 0; n < in_available >> 2; ++n) {
                QUAD_UNPACK(*(uint32_t*)(z_dest + n), re, im);
                if ((int16_t)(re - 1) != rt->global_re || (int16_t)(im - 1) != rt->global_im) {
                    printf("missing samples %d, %d\n", rt->global_re, re);
                    exit(1);
                }
                rt->global_re = re;
                rt->global_im = im;
            }
        }

        if (config->verbose_flag >= 3) {
            int n;
            int16_t re, im;
            for (n = 0; n < in_available >> 2; ++n) {
                QUAD_UNPACK(*(uint32_t*)(z_dest + n), re, im);
                printf("%d, ", re);
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &tl_source);

        // step 4. mix x and z
        if (in_available > 0 && config->mode & WBM_IQ_MIXED) {
            if (config->mode & WBM_AUDIO_TONE) {
                int n;
                int16_t x, foo, re, im;
                for (n = 0; n < in_available >> 2; ++n) {
                    QUAD_UNPACK(sincos16c(rt->x_fcw, &rt->x_phase), x, foo);
                    QUAD_UNPACK(*(uint32_t*)(z_dest + n), re, im);
                    re = ((x + config->x_offset) * re) >> 16;
                    im = ((x + config->x_offset) * im) >> 16;
                    *((uint32_t*)(dest + n)) = QUAD_PACK(re, im);
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &tl_mix);

        // step 5. call write on the whitebox device with how much data was received
        if (in_available > 0) {
            if (write(rt->fd, 0, in_available) != in_available) {
                printf("Underrun");
                fflush(stdout);
                return -1;
            }
            else {
                if (config->verbose_flag) {
                    printf(".");
                    fflush(stdout);
                }
            }
        }
        //else { printf("-"); fflush(stdout); }

        rt->i += in_available >> 2;
        clock_gettime(CLOCK_MONOTONIC, &tl_finish);
        if (config->verbose_flag >= 2) {
            printf("poll=%f sink=%f source=%f mix=%f write=%f (total=%f) processed=%d\n",
                    diff(tl_start, tl_poll),
                    diff(tl_poll, tl_sink),
                    diff(tl_sink, tl_source),
                    diff(tl_source, tl_mix),
                    diff(tl_mix, tl_finish),
                    diff(tl_start, tl_finish),
                    in_available >> 2);
        }
    }

}

void whitebox_finish(whitebox_t *wb, struct whitebox_config *config, struct whitebox_runtime *rt)
{
    fsync(rt->fd);
    whitebox_munmap(wb);
    whitebox_close(wb);

    if (rt->z_fd > 0)
        close(rt->z_fd);

    if (config->z_filename)
        free(config->z_filename);

    printf("Bytes written: %d\n", rt->i << 2);
}

int main(int argc, char **argv)
{
    int fd;
    int q, z, k;
    int16_t a, c, d, g, h;
    uint32_t b;
    struct timespec start, end, final;
    uint16_t entry;
    int opts;

    whitebox_t wb;
    struct whitebox_config current_config, previous_config;
    struct whitebox_config *config = &current_config;
    struct whitebox_runtime rt;

    whitebox_config_init(&current_config);
    whitebox_runtime_init(&rt);

    while (1) {
        int c;
        static struct option long_options[] = {
            { "verbose", no_argument, 0, 'v' },
            { "rate", required_argument, 0, 'r' },
            { "carrier_freq", required_argument, 0, 'c' },
            { "port", required_argument, 0, 'p' },
            { "file", required_argument, 0, 'f' },
            { "tone", required_argument, 0, 't' },
            { "audio_tone", required_argument, 0, 0 },
            { "am", required_argument, 0, 'A' },
            { "duration", required_argument, 0, 'd' },
            { "test", no_argument, 0, 0 },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "r:c:p:f:t:A:d:v",
                long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 0:
                if (strcmp("audio_tone", long_options[option_index].name) == 0) {
                    config->mode = WBM_AUDIO_TONE | WBM_IQ_TONE;
                    config->x_tone = atof(optarg);
                    break;
                }
                if (strcmp("test", long_options[option_index].name) == 0) {
                    config->mode |= WBM_IQ_TEST;
                    break;
                }
                break;
            case 'r':
                config->sample_rate = atoi(optarg);
                break;

            case 'c':
                config->carrier_freq = atof(optarg);
                break;

            case 'v':
                config->verbose_flag++;
                break;

            case 'p':
                config->port = atoi(optarg); 
                rt.z_needs_poll = 1;
                config->mode = WBM_IQ_SOCKET;
                break;

            case 'f':
                config->z_filename = strdup(optarg); 
                rt.z_needs_poll = 1;
                config->mode = WBM_IQ_FILE;
                break;

            case 't':
                config->z_tone = atof(optarg);
                config->mode = WBM_IQ_TONE;
                break;

            case 'A':
                config->x_offset = atoi(optarg);
                break;

            case 'd':
                config->duration = atof(optarg);
                break;

            case '?':
                printf("usage\n");
                return 0;

            default:
                printf("Invalid argument\n");
                return -1;
        }
    }

    if (!(config->mode & 0xf))
        config->mode |= WBM_DEFAULT;

    if(whitebox_start(&wb, config, &rt) < 0)
        return -1;

    clock_gettime(CLOCK_MONOTONIC, &start);

    whitebox_run(&wb, config, &rt);

    whitebox_finish(&wb, config, &rt);

    clock_gettime(CLOCK_MONOTONIC, &end);

    if (config->verbose_flag)
        printf("\nWall time: %f ms\n", diff(start, end));

    return 0;
}
