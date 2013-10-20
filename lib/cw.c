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
#define KEY_GPIO 34

#define TIME_RES_IN_US 10000
#define TIME_RES_IN_MS = (TIME_RES_IN_US/1e3)

int main(int argc, char** argv) {
    // Program level
    char* dev_name = NULL;
    whitebox_t wb;
    int dev_fd;
    int key = 1; // active low

    // Flags
    static int verbose_flag;
    float frequency;

    // Loop
    struct timespec tstart, tend;
    uint32_t alivetime;

    while (1) {
        int c;
        static struct option long_options[] = {
            { "verbose", no_argument, &verbose_flag, 1 },
            { "brief", no_argument, &verbose_flag, 0 },
            { "dev", required_argument, 0, 'd' },
            { "frequency", required_argument, 0, 'f' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "f:v",
                long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
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

    dev_fd = whitebox_open(&wb, dev_name, O_WRONLY, W_DAC_RATE_HZ);
    if (dev_fd < 0) {
        fprintf(stderr, "Couldn't find the whitebox device '%s'.\n", dev_name);
        exit(dev_fd);
    }

    whitebox_tx_set_dds_fcw(50e6);

    whitebox_tx(&wb, frequency);

    if (verbose_flag) {
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
         * Keys
         */
        if (key != GPIO_get_input(KEY_GPIO)) {
            key = !key;
            if (key) { // turn off
                whitebox_tx_flags_disable(&wb, WES_DDSEN);
            } else { // turn on
                whitebox_tx_flags_enable(&wb, WES_DDSEN);
            }
        }

        /*
         * Simulate processing
         */
        do {
            clock_gettime(CLOCK_MONOTONIC, &tend);
            alivetime = (uint32_t)(((tend.tv_sec*1e9 + tend.tv_nsec) - (tstart.tv_sec*1e9 + tstart.tv_nsec))/1e3);
        } while(alivetime < (TIME_RES_IN_US - (TIME_RES_IN_US >> 3)));

        tstart = tend;
    }

    whitebox_tx_flags_disable(&wb, WES_DDSEN);

    whitebox_close(&wb);
    free(dev_name);
}
