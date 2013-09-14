#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "gpio.h"

#define TIME_RES_IN_US 10000
#define TIME_RES_IN_MS = (TIME_RES_IN_US/1e3)
#define SAMPLE_RATE 8000
#define CW_HZ 400
#define SAMPLES_PER_MS (SAMPLE_RATE/1e3)
#define PTT_GPIO 34
#define DAC_RATE 20000000

#define FCW 1000

#define PI 3.1415926

int n; // current sample
int a = 0;
int successful_cycles = 0;
int fd = 0;
int samples_per_res;
uint32_t* samples_buffer;

int read_sample(uint32_t* val) {
    int result = read(fd, val, 4) == 4;
    //int16_t i = *val & 0xffff;
    //fprintf(stderr, "%x\n", i);
    return result;
}


int read_samples(int watch_overrun) {
    int i;
    int result = read(fd, samples_buffer, 4 * samples_per_res) == 4 * samples_per_res;
    if (!result) return 0;

    for (i = 0; i < samples_per_res; ++i) {
        (*((uint32_t volatile*)(0x40050400+0)) = samples_buffer[i]);
        if (watch_overrun && i % 50 == 0) {
            while (exciter_almost_full()) {
                //puts("sleep");
            }
        } else if (!watch_overrun && exciter_almost_full()) {
            break;
        }
    }
    return 1;
}


void exciter_reset() {
    (*((uint32_t volatile*)(0x40050400+4)) = 0x20);
}

void exciter_enable() {
    (*((uint32_t volatile*)(0x40050400+4)) = 0x1);
}

void exciter_disable() {
    (*((uint32_t volatile*)(0x40050400+4)) = 0x0);
}

void exciter_write_samples(uint32_t* samples, size_t nsamples) {
    int i;
    n += nsamples;
    for (i = 0; i < nsamples; ++i)
        (*((uint32_t volatile*)(0x40050400+0)) = samples[i]);
}

uint32_t exciter_samples_written() {
    uint32_t value;
    // TODO: this is a bug in the APB3 slave; somehow pipelining or pready
    // is messing with getting the result in one read.
    (value = *((uint32_t volatile*)(0x40050400+0x10)));
    (value = *((uint32_t volatile*)(0x40050400+0x10)));
    return value;
}

void exciter_print_to_file(FILE* f) {
    uint32_t value;
    // TODO: this is a bug in the APB3 slave; somehow pipelining or pready
    // is messing with getting the result in one read.
    (value = *((uint32_t volatile*)(0x40050400+4)));
    (value = *((uint32_t volatile*)(0x40050400+4)));
    if (value & 0x10) fprintf(f, "almost_full=TRUE\n");
    else fprintf(f, "almost_full=FALSE\n");
    if (value & 0x08) fprintf(f, "almost_empty=TRUE\n");
    else fprintf(f, "almost_empty=FALSE\n");
    if (value & 0x04) fprintf(f, "underrun=TRUE\n");
    else fprintf(f, "underrun=FALSE\n");
    if (value & 0x02) fprintf(f, "overrun=TRUE\n");
    else fprintf(f, "overrun=FALSE\n");
    if (value & 0x01) fprintf(f, "tx_enabled=TRUE\n");
    else fprintf(f, "tx_enabled=FALSE\n");

    (value = *((uint32_t volatile*)(0x40050400+8)));
    (value = *((uint32_t volatile*)(0x40050400+8)));
    fprintf(f, "interp=%d\n", value);
}

void exciter_set_interp(uint32_t interp) {
    (*((uint32_t volatile*)(0x40050400+8)) = interp);
}

int exciter_almost_full() {
    uint32_t value;
    // TODO: this is a bug in the APB3 slave; somehow pipelining or pready
    // is messing with getting the result in one read.
    (value = *((uint32_t volatile*)(0x40050400+4)));
    (value = *((uint32_t volatile*)(0x40050400+4)));
    return (value & 0x10);
}

int exciter_almost_empty() {
    uint32_t value;
    // TODO: this is a bug in the APB3 slave; somehow pipelining or pready
    // is messing with getting the result in one read.
    (value = *((uint32_t volatile*)(0x40050400+4)));
    (value = *((uint32_t volatile*)(0x40050400+4)));
    return (value & 0x08);
}

void prime_samples() {
    uint32_t status;
    int16_t s;
    while (!exciter_almost_full()) {
        //uint32_t val;
        //read_sample(&val);
        //(*((uint32_t volatile*)(0x40050400+0)) = val);
        read_samples(0);
    }

    exciter_print_to_file(stderr);
}

int samples_continue(uint8_t keyed) {
    uint32_t status;
    uint32_t i;
    uint32_t s;
    (status = *((uint32_t volatile*)(0x40050400+4)));
    (status = *((uint32_t volatile*)(0x40050400+4)));
    if (status & 0x04) {
        fprintf(stderr, "U");
        fflush(stderr);
    } else if (status & 0x02) {
        fprintf(stderr, "O");
        fflush(stderr);
    }
    if (!(status & 0x04) && !(status & 0x02)) {
        fprintf(stderr, "%s\n", keyed ? "-" : ".");
        fflush(stderr);
        successful_cycles++;
    }

    if (!read_samples(1)) return 0;

    return 1; // Continue
}

int main(int argc, char** argv) {
    int key = 1; // active low
    int iteration = 0;
    struct timespec tstart, tend;
    uint32_t alivetime;
    n = 0;
    int c;
    static int verbose_flag;
    int rate;
    char* filename;
    uint32_t interp;

    GPIO_config(PTT_GPIO, GPIO_INPUT_MODE);

    while (1) {
        static struct option long_options[] = {
            { "verbose", no_argument, &verbose_flag, 1 },
            { "brief", no_argument, &verbose_flag, 0 },
            { "file", required_argument, 0, 'f' },
            { "rate", required_argument, 0, 'r' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "f:r:",
                long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 'f':
                filename = strdup(optarg);
                break;

            case 'r':
                rate = atoi(optarg);
                break;

            case '?':
                break;

            default:
                break;
        }
    }

    if (DAC_RATE % rate != 0) {
        fprintf(stderr, "Error, sample rate is not a multiple of DAC clock rate!");
        exit(1);
    }

    interp = DAC_RATE / rate;

    samples_per_res = (int)((rate/1e3)*(TIME_RES_IN_US/1e3));
    samples_buffer = malloc(4 * samples_per_res);


    fprintf(stderr, "File: %s\n", filename);
    fprintf(stderr, "Sample Rate: %d\n", rate);
    fprintf(stderr, "Samples per 10ms: %d\n", samples_per_res);
    fprintf(stderr, "Interpolation: %d\n", interp);

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Couldn't open file!\n");
        exit(1);
    }

    exciter_reset();
    exciter_set_interp(interp);
    exciter_print_to_file(stderr);


    //if (!GPIO_get_input(PTT_GPIO)) {
        //cw_on();
        //exciter_enable();
    //} else {
        //cw_on();
        //exciter_enable();
    //}


    prime_samples();
    exciter_enable();

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (++iteration < 1000) {
        if (key != GPIO_get_input(PTT_GPIO)) {
            key = !key;
            if (key) { // turn off
            } else { // turn on
            }
        }

        if (!samples_continue(!key)) break;

        do {
            clock_gettime(CLOCK_MONOTONIC, &tend);
            alivetime = (uint32_t)(((tend.tv_sec*1e9 + tend.tv_nsec) - (tstart.tv_sec*1e9 + tstart.tv_nsec))/1e3);
        } while(alivetime < (TIME_RES_IN_US - (TIME_RES_IN_US >> 3)));

        tstart = tend;
    }

    printf("Successful cycles: %d\n", successful_cycles);

    exciter_disable();
    exciter_print_to_file(stderr);

    free(samples_buffer);
    close(fd);
    free(filename);

}
