/* 
 * Checks that the radio is able to transmit clean sine waves at various
 * frequencies and bitrates.  A spectrum analyzer is required to quantify
 * the results of these tests.
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mman.h>

#include "whitebox.h"
#include "whitebox_test.h"

#define CARRIER_FREQ 144.95e6
#define TONE_FREQ    17e3
#define N 512
#define COUNT 1024 
#define SAMPLE_RATE 10e3 
#define DURATION_IN_SECS 1
#define TOTAL_SAMPLES (DURATION_IN_SECS * SAMPLE_RATE)

int getch() {
    char buf = 0;
    int ret;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        ret = -1;
    else
        ret = buf;
    //    perror ("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO; if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror ("tcsetattr ~ICANON");
    return ret;
}

void confirm(char* msg) {
    int c;
    printf("%s? [yn] ", msg);
    fflush(stdout);
    do
    {
        c = getch();
    } while(c < 0);
    printf("%c\n", (char)c);
    assert((char)c == 'y');
}

void calibrate_dc_offset(int use_dds, int16_t *i, int16_t *q)
{
    float carrier_freq = CARRIER_FREQ;
    float sample_rate = SAMPLE_RATE;
    int ch = -1;
    int n;
    int fd, ret;
    whitebox_t wb;
    uint32_t c[N];

    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    assert(whitebox_tx(&wb, carrier_freq) == 0);
    //whitebox_tx_set_correction(&wb, *i, *q);

    if (use_dds)
        whitebox_tx_dds_enable(&wb, TONE_FREQ);

    for (n = 0; n < N; ++n) {
        c[n] =  0;
    }

    while (ch != 10) {
        ch = getch();
        if (ch > 0) {
            if ((char)ch == 'r') {
                *i = 0; *q = 0;
            }
            else if ((char)ch == 'e') {
                *i = 512; *q = 512;
            }
            else if (ch == 65)
                *i += 1;
            else if (ch == 66)
                *i -= 1;
            else if (ch == 67)
                *q += 1;
            else if (ch == 68)
                *q -= 1;

            printf("i=%d, q=%d\n", *i, *q);
            whitebox_tx_set_correction(&wb, *i, *q);
        }
        ret = write(fd, c, sizeof(uint32_t) * N);
        if (ret != sizeof(uint32_t) * N) {
            printf("U"); fflush(stdout);
        }
    }

    //assert(fsync(fd) == 0);

    assert(whitebox_close(&wb) == 0);
}

void calibrate_gain_and_phase(int16_t i, int16_t q, float *i_gain, float *q_gain, float *phase)
{
    float carrier_freq = CARRIER_FREQ;
    float sample_rate = SAMPLE_RATE;
    int ch = -1;
    int n;
    int fd, ret;
    whitebox_t wb;
    uint32_t c[N];

    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    assert(whitebox_tx(&wb, carrier_freq) == 0);
    whitebox_tx_set_correction(&wb, i, q);

    whitebox_tx_dds_enable(&wb, TONE_FREQ);

    for (n = 0; n < N; ++n) {
        c[n] =  0;
    }

    while (ch != 10) {
        ch = getch();
        if (ch > 0) {
            if ((char)ch == 'r') {
                *i_gain = 1.0; *q_gain = 1.0;
            }
            else if ((char)ch == 'e') {
                *i_gain = 1.9; *q_gain = 1.0;
            }
            if (ch == 65)
                *i_gain += (1 / WEG_COEFF);
            else if (ch == 66)
                *i_gain -= (1 / WEG_COEFF);
            else if (ch == 67)
                *q_gain += (1 / WEG_COEFF);
            else if (ch == 68)
                *q_gain -= (1 / WEG_COEFF);

            printf("i_gain=%f, q_gain=%f\n", *i_gain, *q_gain);
            whitebox_tx_set_gain(&wb, *i_gain, *q_gain);
        }
        ret = write(fd, c, sizeof(uint32_t) * N);
        if (ret != sizeof(uint32_t) * N) {
            printf("U"); fflush(stdout);
        }
    }

    //assert(fsync(fd) == 0);

    assert(whitebox_close(&wb) == 0);
}

void snipe(int use_dds)
{
    whitebox_t wb;
    whitebox_args_t w;
    int ret, i = 0, new_i = 0;
    int last_count = 0;
    int ch = -1;
    unsigned long dest, count;
    void *wbptr;
    int buffer_size = sysconf(_SC_PAGE_SIZE)
            << whitebox_parameter_get("user_order");
    float fstart=144e6;
    float fstop=148e6;
    float fstep=4e6;

    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE) > 0);
    wbptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, wb.fd, 0);
    assert(wbptr != MAP_FAILED && wbptr);

    assert(whitebox_tx(&wb, fstart) == 0);

    if (use_dds)
        whitebox_tx_dds_enable(&wb, TONE_FREQ);

    while (ch != 10) {
        ch = getch();
        if ((char)ch == 'r')
            new_i = 0;
        else if (ch == 65)
            new_i = i + 1;
        else if (ch == 66)
            new_i = i - 1;
        else if (last_count >= TOTAL_SAMPLES) {
            last_count = 0;
            new_i = i + 1;
        } else
            new_i = i;

        if (new_i != i) {
            i = new_i;
            fsync(wb.fd);
            whitebox_tx_fine_tune(&wb, fstart + i * fstep);
        }

        count = ioctl(wb.fd, W_MMAP_WRITE, &dest) >> 2;
        if (count <= 0) {
            continue;
        } else {
            count = count < COUNT ? count : COUNT;
            // DSP
            ret = write(whitebox_fd(&wb), 0, count << 2);
            if (ret != count << 2) {
                whitebox_debug_to_file(&wb, stdout);
                perror("write: ");
            }
            assert(ret == count << 2);
            last_count += count;
        }

    }

    fsync(wb.fd);

    assert(munmap(wbptr, buffer_size) == 0);
    assert(whitebox_close(&wb) == 0);
}

int main(int argc, char **argv) {
    whitebox_t wb;
    int16_t i, q;
    float i_gain, q_gain, phase;

    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE) > 0);
    whitebox_tx_get_correction(&wb, &i, &q);
    whitebox_tx_get_gain(&wb, &i_gain, &q_gain);
    assert(whitebox_close(&wb) == 0);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    printf("Step 0. Choose carrier freq\n");
    snipe(0);
    printf("Step 1. Static DC Offset\n");
    calibrate_dc_offset(0, &i, &q);
    printf("Step 2. SSB DC Offset\n");
    calibrate_dc_offset(1, &i, &q);
#if 0
    printf("Step 3. SSB Gain/Phase Mismatch\n");
    calibrate_gain_and_phase(i, q, &i_gain, &q_gain, &phase);
#endif

    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_i_correction\n", i);
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_q_correction\n", q);
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_i_gain\n", (uint16_t)(i_gain * WEG_COEFF + 0.5));
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_q_gain\n", (uint16_t)(q_gain * WEG_COEFF + 0.5));
    //printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_phase\n", phase);
}
