// GPIO 34 is the button, in, active_low

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "gpio.h"

#define TIME_RES_IN_US 10000
#define TIME_RES_IN_MS = (TIME_RES_IN_US/1e3)
#define SAMPLE_RATE 8000
#define CW_HZ 400
#define SAMPLES_PER_MS (SAMPLE_RATE/1e3)
#define SAMPLES_PER_RES (int)((SAMPLE_RATE/1e3)*(TIME_RES_IN_US/1e3))
#define KEY_GPIO 34

#define FCW 1000

#define PI 3.1415926

int n; // current sample
int a = 0;
int successful_cycles = 0;

void exciter_reset() {
    (*((uint32_t volatile*)(0x40050400+4)) = 0x20);
}

void exciter_enable() {
    (*((uint32_t volatile*)(0x40050400+4)) = 0x41);
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
    if (value & 0x40) fprintf(f, "source=DDS\n");
    else fprintf(f, "source=SAMPLES\n");
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

void cw_on() {
    uint32_t status;
    int16_t s;
    puts("cw_on");
    while (!exciter_almost_full()) {
        //s = (int16_t)(sin(CW_HZ * (2 * PI) * n++ / SAMPLE_RATE));
        s = 0;
        (*((uint32_t volatile*)(0x40050400+0)) = s);
    }

    exciter_print_to_file(stderr);
}

void cw_off() {
    int32_t n0;
    int16_t s;
    /*while (exciter_almost_full()) {
        usleep(10);
    }*/
    puts("cw_off");
    for (n0 = n; n - n0 < SAMPLES_PER_RES; ++n) {
        s = 0;
        (*((uint32_t volatile*)(0x40050400+0)) = s);
    }
}

void cw_continue(uint8_t keyed) {
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

    for (i = 0; i < SAMPLES_PER_RES; ++i, n+=FCW) {
        //if (keyed) s = (int16_t)((2 << 15) * sin(CW_HZ * (2 * PI) * n++ / SAMPLE_RATE));
        //if (keyed) s = n << 8;
        //if (keyed) {if (s == 0x7fff7fff) s = 0xffffffff; else s = 0x7fff7fff;}
        if (keyed) s = 0x7fff7fff;
        else s = 0;
        //s += n;
        (*((uint32_t volatile*)(0x40050400+0)) = s);
        //(*((uint32_t volatile*)(0x40050400+0)) = 0xffffffff);
        if (i % 50 == 0) {
            while (exciter_almost_full()) {
                //puts("sleep");
            }
        }
    }
}

int main(int argc, char** argv) {
    int key = 1; // active low
    int iteration = 0;
    struct timespec tstart, tend;
    uint32_t alivetime;
    n = 0;

    puts("CW");
    GPIO_config(KEY_GPIO, GPIO_INPUT_MODE);

    exciter_reset();
    exciter_set_interp(2500);

    printf("Exciter samples per res: %d\n", SAMPLES_PER_RES);
    printf("Exciter samples: %d\n", exciter_samples_written());

    /*cw_on();
    exciter_print_to_file(stderr);
    exciter_enable();*/

    if (!GPIO_get_input(KEY_GPIO)) {
        //cw_on();
        exciter_enable();
    } else {
        //cw_on();
        exciter_enable();
    }

    exciter_print_to_file(stderr);

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    while (++iteration < 100000) {
        /*if (iteration % 100 == 0) {
            exciter_print_to_file(stderr);
        }*/

        if (key != GPIO_get_input(KEY_GPIO)) {
            key = !key;
            if (key) { // turn off
            } else { // turn on
            }
        }

        //cw_continue(!key);

        do {
            clock_gettime(CLOCK_MONOTONIC, &tend);
            alivetime = (uint32_t)(((tend.tv_sec*1e9 + tend.tv_nsec) - (tstart.tv_sec*1e9 + tstart.tv_nsec))/1e3);
        } while(alivetime < (TIME_RES_IN_US - (TIME_RES_IN_US >> 3)));

        tstart = tend;
    }

    printf("Successful cycles: %d\n", successful_cycles);

    exciter_disable();
    exciter_print_to_file(stderr);

}
