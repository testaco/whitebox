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

int main(int argc, char** argv) {
    // Program level
    whitebox_t wb;
    char* dev_name = NULL;
    int dev_fd;
    int samples_per_res;
    uint32_t* samples_buffer;

    // Flags
    static int verbose_flag;
    static int use_filter = 0;
    static int loop_flag = 0;
    int rate;
    char* filename;
    float frequency;

    // Loop
    int fd = 0;
    struct timespec tstart, tend;
    uint32_t alivetime;

    while (1) {
        int c;
        static struct option long_options[] = {
            { "verbose", no_argument, &verbose_flag, 1 },
            { "brief", no_argument, &verbose_flag, 0 },
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

    whitebox_parameter_set("check_plls", 1);
    whitebox_parameter_set("check_runs", 1);
    whitebox_parameter_set("mock_en", 0);
    whitebox_init(&wb);
    dev_fd = whitebox_open(&wb, dev_name, O_RDWR, rate);
    if (dev_fd < 0) {
        fprintf(stderr, "Couldn't find the whitebox device '%s'.\n", dev_name);
        exit(dev_fd);
    }

    if (whitebox_mmap(&wb) < 0) {
        fprintf(stderr, "Couldn't mmap whitebox device\n");
        exit(1);
    }

    samples_per_res = (int)((rate/1e3)*(TIME_RES_IN_US/1e3));

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

    if (use_filter) {
        whitebox_tx_flags_enable(&wb, WES_FILTEREN);
    } else {
        whitebox_tx_flags_disable(&wb, WES_FILTEREN);
    }

    whitebox_tx(&wb, frequency);

    if (verbose_flag) {
        fprintf(stderr, "file=%s\n", filename);
        fprintf(stderr, "sample_rate=%d\n", rate);
        fprintf(stderr, "samples_per_frame=%d\n", samples_per_res);
        fprintf(stderr, "page_size=%d\n", sysconf(_SC_PAGESIZE));
        fprintf(stderr, "filter=%s\n", use_filter ? "on" : "off");
        whitebox_debug_to_file(&wb, stderr);
    }

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (1) {
        int rcnt, wcnt;
        uint32_t *tx_ptr;
        long tx_count, tx_orig;

        tx_orig = tx_count = ioctl(dev_fd, W_MMAP_WRITE, &tx_ptr) >> 2;
        //tx_count = tx_count < samples_per_res ? tx_count : samples_per_res;
        if (tx_count == 0) {
            fprintf(stderr, "Warning we're full\n");
            continue;
        }

        /*
         * Read from the input file
         */
        rcnt = read(fd, tx_ptr, sizeof(uint32_t) * tx_count);
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
        tx_count = rcnt >> 2;

        /*
         * Write to the whitebox device
         */
        wcnt = write(dev_fd, tx_ptr, sizeof(uint32_t) * tx_count);
        if (wcnt < 0) {
            perror("Error writing to whitebox device");
            break;
        } else if (rcnt != wcnt) {
            fprintf(stderr, "Warning, read and write counts don't match original %d read %d wrote %d\n", tx_count << 2, rcnt, wcnt);
        }

        fprintf(stderr, ".");
        fflush(stderr);

        /*
         * Simulate processing
         */
         #if 0
        do {
            clock_gettime(CLOCK_MONOTONIC, &tend);
            alivetime = (uint32_t)(((tend.tv_sec*1e9 + tend.tv_nsec) - (tstart.tv_sec*1e9 + tstart.tv_nsec))/1e3);
        } while(alivetime < (TIME_RES_IN_US - (TIME_RES_IN_US >> 3)));

        tstart = tend;
        #endif
    }

    fsync(wb.fd);

    close(fd);
    whitebox_munmap(&wb);
    whitebox_close(&wb);
    free(dev_name);
}
