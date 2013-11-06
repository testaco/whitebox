#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <sys/mman.h>

#include "whitebox.h"

#define TIME_RES_IN_US 10000
#define TIME_RES_IN_MS = ((int)(TIME_RES_IN_US/1e3))

// Flags
typedef struct udp_peripheral {
    whitebox_t* wb;
    int udpfd;
    int state;

    char* dev_name;

    int verbose_flag;
    int use_filter;

    float frequency;

    int rate;
    int samples_per_res;
    uint32_t* samples_buffer;

    uint16_t cur_overruns, cur_underruns;
} udp_peripheral_t;

void udp_peripheral_init(udp_peripheral_t* p, whitebox_t* wb, int rate) {
    p->wb = wb;
    p->dev_name = NULL;
    p->state = 0;

    p->rate = rate;
    p->samples_per_res = (int)((rate/1e3)*(TIME_RES_IN_US/1e3));
    //p->samples_buffer = malloc(sizeof(uint32_t) * p->samples_per_res);

    //memset(p->samples_buffer, 0, sizeof(uint32_t) * p->samples_per_res);
}

void udp_peripheral_init_getopt(udp_peripheral_t* p, whitebox_t* wb,
        int argc, char** argv) {
    int rate;

    while (1) {
        int c;
        struct option long_options[] = {
            { "verbose", no_argument, &p->verbose_flag, 1 },
            { "brief", no_argument, &p->verbose_flag, 0 },
            { "filter", no_argument, &p->use_filter, 1 },
            { "rate", required_argument, 0, 'r' },
            { "dev", required_argument, 0, 'd' },
            { "frequency", required_argument, 0, 'f' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "r:d:f:v",
                long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 'r':
                rate = atoi(optarg);
                break;

            case 'd':
                p->dev_name = strdup(optarg);
                break;

            case 'f':
                p->frequency = atof(optarg);

            case 'v':
                p->verbose_flag = 1;
                break;

            case '?':
                break;

            default:
                break;
        }
    }

    udp_peripheral_init(p, wb, rate);
}

void udp_peripheral_free(udp_peripheral_t* p) {
    //free(p->samples_buffer);
    //p->samples_buffer = NULL;
    free(p->dev_name);
    p->dev_name = NULL;
}

int udp_peripheral_open_device(udp_peripheral_t* p) {
    int dev_fd;
    whitebox_init(p->wb);

    dev_fd = whitebox_open(p->wb, p->dev_name, O_RDWR | O_NONBLOCK, p->rate);
    if (dev_fd < 0) {
        fprintf(stderr, "Couldn't find the whitebox device '%s'.\n",
                p->dev_name);
        exit(dev_fd);
    }

    p->samples_buffer = mmap(0,
            whitebox_tx_get_ring_buffer_size(p->wb),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            dev_fd,
            0);

    if (p->samples_buffer == MAP_FAILED) {
        fprintf(stderr, "value was %d\n", errno);
        perror("mmap");
        exit(errno);
    }

    fprintf(stderr, "value is %x\n", (unsigned long)p->samples_buffer);

    whitebox_tx_set_buffer_threshold(p->wb,
            WE_FIFO_SIZE-(3*p->samples_per_res)-1,
            WE_FIFO_SIZE-p->samples_per_res-1);

    if (p->use_filter) {
        whitebox_tx_flags_enable(p->wb, WES_FILTEREN);
    } else {
        whitebox_tx_flags_disable(p->wb, WES_FILTEREN);
    }

    whitebox_tx_get_buffer_runs(p->wb, &p->cur_overruns, &p->cur_underruns);

    whitebox_tx(p->wb, p->frequency);

    return dev_fd;
}

int udp_peripheral_open_udp(udp_peripheral_t* p) {
    // UDP Socket
    struct sockaddr_in my_addr, cli_addr;
    socklen_t slen=sizeof(cli_addr);
    struct servent *serv;

    if ((p->udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stderr, "Socket\n");
        exit(p->udpfd);
    }

    serv = getservbyname("gnuradio", "udp");
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = serv->s_port;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    fprintf(stderr, "listening on %d\n", serv->s_port);

    if (bind(p->udpfd, (struct sockaddr*) &my_addr, sizeof(my_addr)) < 0) {
        fprintf(stderr, "Socket\n");
        exit(-1);
    }
    return p->udpfd;
}

void udp_peripheral_close(udp_peripheral_t* p) {
    close(p->udpfd);
    munmap(p->samples_buffer, whitebox_tx_get_ring_buffer_size(p->wb));
    whitebox_close(p->wb);
}

int udp_peripheral_loop(udp_peripheral_t* p) {
    // Loop
    struct pollfd poll_list[2];

    int rcnt, wcnt, pret;
    uint16_t overruns, underruns;

    poll_list[0].fd = whitebox_fd(p->wb);
    poll_list[0].events = POLLOUT;
    poll_list[1].fd = p->udpfd;
    poll_list[1].events = POLLIN;


    /*
     * Make sure that the PLL's are still locked.
     */
    if (!whitebox_plls_locked(p->wb)) {
        fprintf(stderr, "L");
        fflush(stderr);
    }

    /*
     * Check the status of the exciter buffer to see if we're
     * overrunning / underruning.
     */
    whitebox_tx_get_buffer_runs(p->wb, &overruns, &underruns);

    if (p->cur_overruns != overruns) {
        p->cur_overruns = overruns;
        fprintf(stderr, "O");
        fflush(stderr);
    }
    if (p->cur_underruns != underruns) {
        p->cur_underruns = underruns;
        fprintf(stderr, "U");
        fflush(stderr);
    }

    /*
     * Poll the udp socket and whitebox device.
     */
    pret = poll(poll_list, 2, 10);
    if (pret < 0) {
        perror("Error polling");
        exit(pret);
    }

    // WHOOPS NOW HERE'S THE PROBLEM: SAMPLES_BUFFER ISN'T FILLED
    // JUST YET, so the loop needs to take that into account; it

    /*
     * Read from the input file
     *
    if (poll_list[1].revents & POLLIN) {
        rcnt = recvfrom(poll_list[1].fd,
            p->samples_buffer, sizeof(uint32_t) * p->samples_per_res,
            0, (struct sockaddr*) &cli_addr, &slen);
        if (rcnt < 0) {
            perror("Error reading from socket");
            exit(rcnt);
        } else if (rcnt == 0) {
            break;
        } else if (rcnt != sizeof(uint32_t) * p->samples_per_res) {
            fprintf(stderr, "!=\n");
        }
    }*/

    /*
     * Write to the whitebox device
     *
    if (poll_list[0].revents & POLLOUT) {
        wcnt = write(poll_list[0].fd, p->samples_buffer, rcnt);
        if (wcnt < 0) {
            perror("Error writing to whitebox device");
        } else if (rcnt != wcnt) {
            fprintf(stderr, "Warning, read and write counts don't match\n");
        }
    }*/

    fprintf(stderr, ".");
    fflush(stderr);

}

int udp_peripheral_main(udp_peripheral_t* p) {
    // Main level
    int ret = 0;
    struct timespec tstart, tend;
    uint32_t alivetime;

    udp_peripheral_open_device(p);
    udp_peripheral_open_udp(p);

    return 0;

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (1) {
        
        ret = udp_peripheral_loop(p);
        if (ret)
            goto done;

        clock_gettime(CLOCK_MONOTONIC, &tend);
        alivetime = (uint32_t)(((tend.tv_sec*1e9 + tend.tv_nsec) - (tstart.tv_sec*1e9 + tstart.tv_nsec))/1e3);
        tstart = tend;
    }
    
done:
    udp_peripheral_close(p);
    udp_peripheral_free(p);

    return p->state;
}

void udp_peripheral_print_to_file(udp_peripheral_t* p, FILE* f) {
    if (p->verbose_flag) {
        fprintf(stderr, "sample_rate=%d\n", p->rate);
        fprintf(stderr, "samples_per_frame=%d\n", p->samples_per_res);
        fprintf(stderr, "page_size=%d\n", sysconf(_SC_PAGESIZE));
        whitebox_print_to_file(p->wb, stderr);
    }
}

int main(int argc, char** argv) {
    // Program level
    whitebox_t wb;
    udp_peripheral_t p;
    
    udp_peripheral_init_getopt(&p, &wb, argc, argv);

    udp_peripheral_print_to_file(&p, stderr);

    return udp_peripheral_main(&p);
}
