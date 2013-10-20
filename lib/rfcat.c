#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "whitebox.h"

#define TIME_RES_IN_US 10000
#define TIME_RES_IN_MS = (TIME_RES_IN_US/1e3)
#define DAC_RATE_HZ 20000000

int main(int argc, char** argv) {
    // Program level
    whitebox_t wb;
    char* dev_name = NULL;
    int dev_fd;
    int samples_per_res;
    uint32_t* samples_buffer;

    // Flags
    static int verbose_flag;
    static int use_dds = 0;
    static int use_filter = 0;
    static int loop_flag = 0;
    int rate;
    char* filename;
    float frequency;

    // Loop
    int fd = 0;
    uint16_t cur_overruns, cur_underruns;
    struct timespec tstart, tend;
    uint32_t alivetime;

    while (1) {
        int c;
        static struct option long_options[] = {
            { "verbose", no_argument, &verbose_flag, 1 },
            { "brief", no_argument, &verbose_flag, 0 },
            { "dds", no_argument, &use_dds, 1 },
            { "filter", no_argument, &use_filter, 1 },
            { "rate", required_argument, 0, 'r' },
            { "dev", required_argument, 0, 'd' },
            { "frequency", required_argument, 0, 'f' },
            { "loop", no_argument, &loop_flag, 1 },
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
                dev_name = strdup(optarg);
                break;

            case 'f':
                frequency = atof(optarg);

            case 'v':
                verbose_flag = 1;
                break;

            case '?':
                break;

            default:
                break;
        }
    }

    dev_fd = whitebox_open(&wb, dev_name, O_WRONLY, rate);
    if (dev_fd < 0) {
        fprintf(stderr, "Couldn't find the whitebox device '%s'.\n", dev_name);
        exit(dev_fd);
    }

    samples_per_res = (int)((rate/1e3)*(TIME_RES_IN_US/1e3));
    samples_buffer = malloc(sizeof(uint32_t) * samples_per_res);

    if (!samples_buffer) {
        fprintf(stderr, "Error, couldn't allocate samples buffer.");
        exit(1);
    }

    memset(samples_buffer, 0, sizeof(uint32_t) * samples_per_res);
    //for (i = 0; i < samples_per_res; ++i)
    //    samples_buffer[i] = 0x00007fff;

    if (optind >= argc) {
        fd = 0;
        filename = "stdin";
    } else {
        filename = argv[optind++];
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Couldn't open file '%s'!\n", filename);
            exit(fd);
        }
    }

    whitebox_tx_set_buffer_threshold(&wb,
            WE_FIFO_SIZE-(3*samples_per_res)-1,
            WE_FIFO_SIZE-samples_per_res-1);

    whitebox_tx_set_dds_fcw(50e6);

    if (use_dds)
        whitebox_tx_flags_enable(&wb, WES_DDSEN);
    else if (use_filter) {
        whitebox_tx_flags_enable(&wb, WES_FILTEREN);
    } else {
        whitebox_tx_flags_disable(&wb, WES_FILTEREN);
    }

    whitebox_tx_get_buffer_runs(&wb, &cur_overruns, &cur_underruns);

    whitebox_tx(&wb, frequency);

    if (verbose_flag) {
        fprintf(stderr, "file=%s\n", filename);
        fprintf(stderr, "sample_rate=%d\n", rate);
        fprintf(stderr, "samples_per_frame=%d\n", samples_per_res);
        fprintf(stderr, "page_size=%d\n", sysconf(_SC_PAGESIZE));
        whitebox_print_to_file(&wb, stderr);
    }

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (1) {
        int rcnt, wcnt;
        uint16_t overruns, underruns;

        /*
         * Make sure that the PLL's are still locked.
         */
        if (!whitebox_plls_locked(&wb)) {
            fprintf(stderr, "L");
            fflush(stderr);
        }

        /*
         * Check the status of the exciter buffer to see if we're
         * overrunning / underruning.
         */
        whitebox_tx_get_buffer_runs(&wb, &overruns, &underruns);

        if (cur_overruns != overruns) {
            cur_overruns = overruns;
            fprintf(stderr, "O");
            fflush(stderr);
        }
        if (cur_underruns != underruns) {
            cur_underruns = underruns;
            fprintf(stderr, "U");
            fflush(stderr);
        }

        /*
         * Read from the input file
         */
        rcnt = read(fd, samples_buffer, sizeof(uint32_t) * samples_per_res);
        if (rcnt < 0) {
            perror("Error reading from file");
            exit(rcnt);
        } else if (rcnt == 0) {
            if (loop_flag) {
                lseek(fd, 0, SEEK_SET);
                continue;
            } else {
                break;
            }
        }

        /*
         * Write to the whitebox device
         */
        wcnt = write(dev_fd, samples_buffer, rcnt);
        if (wcnt < 0) {
            perror("Error writing to whitebox device");
        } else if (rcnt != wcnt) {
            fprintf(stderr, "Warning, read and write counts don't match\n");
        }

        fprintf(stderr, ".");
        fflush(stderr);

        /*
         * Simulate processing
         */
        do {
            clock_gettime(CLOCK_MONOTONIC, &tend);
            alivetime = (uint32_t)(((tend.tv_sec*1e9 + tend.tv_nsec) - (tstart.tv_sec*1e9 + tstart.tv_nsec))/1e3);
        } while(alivetime < (TIME_RES_IN_US - (TIME_RES_IN_US >> 3)));

        tstart = tend;
    }

    if (use_dds)
        whitebox_tx_flags_disable(&wb, WES_DDSEN);

    close(fd);
    free(samples_buffer);
    whitebox_close(&wb);
    free(dev_name);
}
