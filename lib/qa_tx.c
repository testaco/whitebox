/* 
 * Checks that the radio is able to transmit clean sine waves at various
 * frequencies and bitrates.  A spectrum analyzer is required to quantify
 * the results of these tests.
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mman.h>
#include <string.h>

#include "whitebox.h"
#include "whitebox_test.h"

#define CARRIER_FREQ 145.00e6
#define TONE_FREQ    7e3
#define N 512
#define COUNT 1024 
#define SAMPLE_RATE 48e3 
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
    unsigned long dest, count;
    uint32_t phase = 0;
    uint32_t fcw = freq_to_fcw(TONE_FREQ, sample_rate);

    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    assert(whitebox_mmap(&wb) == 0);
    assert(whitebox_tx(&wb, carrier_freq) == 0);

    while (ch != 10) {
        ch = getch();
        if (ch > 0) {
            if ((char)ch == 'r') {
                *i = 0; *q = 0;
            }
            else if ((char)ch == 'e') {
                *i = 512; *q = 512;
            }
            else if ((char)ch == 's') {
                int fd = open("/sys/power/state", O_WRONLY);
                write(fd, "standby\n", strlen("standby\n"));
                sleep(10);
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

        count = ioctl(fd, W_MMAP_WRITE, &dest) >> 2;
        if (count <= 0)
            continue;

        count = count < COUNT ? count : COUNT;
        if (use_dds) {
            int j;
            for (j = 0; j < count; ++j) {
                int16_t re, im;
                QUAD_UNPACK(sincos16c(fcw, &phase), re, im);
                ((uint32_t*)dest)[j] = QUAD_PACK(re, im);
            }
        } else {
            memset((void*)dest, 0, count << 2);
        }
        ret = write(fd, 0, count << 2);
        if (ret != count << 2) {
            printf("U"); fflush(stdout);
        }
    }

    //assert(fsync(fd) == 0);
    assert(whitebox_munmap(&wb) == 0);
    assert(whitebox_close(&wb) == 0);
}

void calibrate_gain_and_phase(int16_t i, int16_t q, float *i_gain, float *q_gain)
{
    float carrier_freq = CARRIER_FREQ;
    float sample_rate = SAMPLE_RATE;
    int ch = -1;
    int n;
    int fd, ret;
    int j;
    whitebox_t wb;
    unsigned long dest, count;
    uint32_t phase = 0;
    uint32_t fcw = freq_to_fcw(TONE_FREQ, sample_rate);

    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    assert(whitebox_mmap(&wb) == 0);
    assert(whitebox_tx(&wb, carrier_freq) == 0);
    whitebox_tx_set_correction(&wb, i, q);

    while (ch != 10) {
        ch = getch();
        if (ch > 0) {
            if ((char)ch == 'r') {
                *i_gain = 1.0; *q_gain = 1.0;
            }
            else if ((char)ch == 'z') {
                *i_gain = *q_gain = 0.0;
            }
            else if ((char)ch == 'h') {
                *i_gain = 0.5; *q_gain = 0.5;
            }
            else if ((char)ch == 'j') {
                *i_gain -= (1 / WEG_COEFF);
                *q_gain -= (1 / WEG_COEFF);
            }
            else if ((char)ch == 'k') {
                *i_gain += (1 / WEG_COEFF);
                *q_gain += (1 / WEG_COEFF);
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

        count = ioctl(fd, W_MMAP_WRITE, &dest) >> 2;
        if (count <= 0)
            continue;

        count = count < COUNT ? count : COUNT;

        for (j = 0; j < count; ++j) {
            int16_t re, im;
            QUAD_UNPACK(sincos16c(fcw, &phase), re, im);
            ((uint32_t*)dest)[j] = QUAD_PACK(re, im);
        }

        ret = write(fd, 0, count << 2);
        if (ret != count << 2) {
            printf("U"); fflush(stdout);
        }
    }

    //assert(fsync(fd) == 0);

    assert(whitebox_munmap(&wb) == 0);
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

int whitebox_tx2(whitebox_t* wb, float frequency, int m, int n) {
    float vco_frequency;
    whitebox_args_t w;

    ioctl(wb->fd, WC_GET, &w);
    cmx991_ioctl_get(&wb->cmx991, &w);
    cmx991_resume(&wb->cmx991);
    if (cmx991_pll_enable_m_n(&wb->cmx991, 19.2e6, m, n) < 0) {
        fprintf(stderr, "Error setting the pll\n");
        return 1;
    }

    vco_frequency = (frequency + 45.00e6) * 4.0;
    if (vco_frequency <= 35.00e6) {
        fprintf(stderr, "VCO frequency too low\n");
        return 2;
    }

    cmx991_tx_tune(&wb->cmx991, vco_frequency,
        IF_FILTER_BW_45MHZ, HI_LO_LOWER,
        TX_RF_DIV_BY_4, TX_IF_DIV_BY_4, GAIN_P0DB);
    cmx991_ioctl_set(&wb->cmx991, &w);
    ioctl(wb->fd, WC_SET, &w);

    adf4351_init(&wb->adf4351);

    adf4351_pll_enable(&wb->adf4351, WA_CLOCK_RATE, 8e3, vco_frequency);
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);
    return 0;
}

void test_iflo(void) {
    whitebox_t wb;
    whitebox_args_t w;
    char ch;
    int m = 192, n = 1800;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE) > 0);

    whitebox_tx2(&wb, CARRIER_FREQ, m, n);

    while (ch != 10) {
        ch = getch();
        if (ch == 65)
            m += 10;
        else if (ch == 66)
            m -= 10;
        else if (ch == 67)
            n += 10;
        else if (ch == 68)
            n -= 10;

        if (ch >= 65 && ch <= 68) {
            whitebox_tx2(&wb, CARRIER_FREQ, m, n);
            printf("m=%d, n=%d, f=%f\n", m, n,
                cmx991_pll_actual_frequency(&wb.cmx991, 19.2e6));
        }
    }
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
    //snipe(0);
    //test_iflo();
    printf("Step 1. Static DC Offset\n");
    calibrate_dc_offset(0, &i, &q);
    printf("Step 2. SSB DC Offset\n");
    calibrate_dc_offset(1, &i, &q);
    printf("Step 3. SSB Gain/Phase Mismatch\n");
    calibrate_gain_and_phase(i, q, &i_gain, &q_gain);

    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_i_correction\n", i);
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_q_correction\n", q);
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_i_gain\n", (uint16_t)(i_gain * WEG_COEFF + 0.5));
    printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_q_gain\n", (uint16_t)(q_gain * WEG_COEFF + 0.5));
    //printf("echo %d > /sys/module/whitebox/parameters/whitebox_tx_phase\n", phase);
}
