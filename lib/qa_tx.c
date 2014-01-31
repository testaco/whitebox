/* 
 * Checks that the radio is able to transmit clean sine waves at various
 * frequencies and bitrates.  A spectrum analyzer is required to quantify
 * the results of these tests.
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>

#include "whitebox.h"
#include "whitebox_test.h"

#define CARRIER_FREQ 144.95e6
#define TONE_FREQ    17e3
#define N 512
#define SAMPLE_RATE 10e3 
#define DURATION_IN_SECS 3
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
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
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

int test_dds(void *data) {
    whitebox_t wb;
    int fd;
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, 1e6)) > 0);
    confirm("Is there no carrier at 144.95e6");
    whitebox_tx_dds_enable(&wb, TONE_FREQ);
    confirm("Is there a USB tone visible now");
    assert(whitebox_tx(&wb, CARRIER_FREQ) == 0);
    confirm("Is there a carrier at 144.95e6");
    whitebox_close(&wb);
    confirm("Is there no more signal");
}

int test_cic(void * data) {
    float freq = SAMPLE_RATE / 4;
    float sample_rate = SAMPLE_RATE;
    float carrier_freq = CARRIER_FREQ;
    uint32_t fcw = freq_to_fcw(freq, sample_rate);
    uint32_t last_phase = 0;
    uint32_t phases[N];
    uint32_t c[N];
    int16_t i, q;
    int n;
    int fd, ret;
    whitebox_t wb;

    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    assert(whitebox_tx(&wb, carrier_freq) == 0);
    whitebox_debug_to_file(&wb, stdout);

    /*uint32_t phase_sequence = {
        0 << 30,
        1 << 30,
        2 << 30,
        3 << 30
    };*/

    whitebox_tx_dds_enable(&wb, TONE_FREQ);

    while (1) {

        for (i = -53; i <= -47; i += 1) {
            /*for (n = 0; n < N; ++n) {
                c[n] =  ((uint32_t)i & 0x0000ffff) << 16;
            }*/

            printf("%d\n", i);
            whitebox_tx_set_correction(&wb, 16, -51);
            for (n = 0; n < TOTAL_SAMPLES; n += N) {
                ret = write(whitebox_fd(&wb), c, sizeof(uint32_t) * N);
                if (ret != sizeof(uint32_t) * N) {
                    printf("U"); fflush(stdout);
                }
            }
        }

    }

    assert(fsync(fd) == 0);

    assert(whitebox_close(&wb) == 0);
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
    float carrier_freq = 144.95e6;
    float sample_rate = SAMPLE_RATE;
    int ch = -1;
    int n;
    int fd, ret;
    whitebox_t wb;
    uint32_t c[N];
    int mode = 0;
    int max_mode = 4;
    char *mode_str[100] = {
        "low_noise",
        "reserved0",
        "reserved1",
        "low_spur",
    };

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
            if ((char)ch == 'r')
                carrier_freq = 144.95e6;
            else if (ch == 65)
                carrier_freq += 500e3;
            else if (ch == 66)
                carrier_freq -= 500e3;
            /*else if (ch == 67)
                *q += 1;
            else if (ch == 68)
                *q -= 1;*/

            printf("%f\n", carrier_freq);
            whitebox_tx_clear(&wb);
            assert(whitebox_tx(&wb, carrier_freq) == 0);
            //whitebox_tx_set_correction(&wb, *i, *q);
        }
        ret = write(fd, c, sizeof(uint32_t) * N);
        if (ret != sizeof(uint32_t) * N) {
            printf("U"); fflush(stdout);
        }
    }

    //assert(fsync(fd) == 0);

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
    printf("Step 0. Snipe something\n");
    snipe(0);
    printf("Step 1. Static DC Offset\n");
    calibrate_dc_offset(0, &i, &q);
    printf("Step 2. SSB DC Offset\n");
    calibrate_dc_offset(1, &i, &q);
    printf("Step 3. SSB Gain/Phase Mismatch\n");
    calibrate_gain_and_phase(i, q, &i_gain, &q_gain, &phase);

    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_i_correction\n", i);
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_q_correction\n", q);
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_i_gain\n", (uint16_t)(i_gain * WEG_COEFF + 0.5));
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_q_gain\n", (uint16_t)(q_gain * WEG_COEFF + 0.5));
    //printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_phase\n", phase);
}
