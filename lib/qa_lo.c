/*
 * Checks that the radio frequency local oscillator (LO) can span across
 * the bands and steps we want.
 */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mman.h>

#include "whitebox.h"
#include "whitebox_test.h"

#define COUNT 1024 
#define SAMPLE_RATE 10e3 
#define DURATION_IN_SECS .5
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

void tune_accross_band(whitebox_t* wb, float fstart, float fstop, float fstep)
{
    whitebox_args_t w;
    int ret, i = 0, new_i = 0;
    int last_count = 0;
    int ch = -1;
    int steps = (fstop - fstart) / fstep;
    unsigned long dest, count;

    /*adf4351_init(&wb->adf4351);
    adf4351_pll_enable(&wb->adf4351, WA_CLOCK_RATE, fstep, fstart);
    adf4351_print_to_file(&wb->adf4351, stdout);
    adf4351_ioctl_set(&wb->adf4351, &w);
    ioctl(wb->fd, WA_SET, &w);*/
    whitebox_tx(wb, fstart);

    //for (i = 0; i < steps; ++i) {
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
            fsync(wb->fd);
            whitebox_tx_fine_tune(wb, fstart + i * fstep);
        }

        count = ioctl(wb->fd, W_MMAP_WRITE, &dest) >> 2;
        if (count <= 0) {
            continue;
        } else {
            count = count < COUNT ? count : COUNT;
            // DSP
            ret = write(whitebox_fd(wb), 0, count << 2);
            if (ret != count << 2) {
                whitebox_debug_to_file(wb, stdout);
                perror("write: ");
            }
            assert(ret == count << 2);
            last_count += count;
        }

    }

    fsync(wb->fd);
}

int main(int argc, char **argv) {
    whitebox_t wb;
    void *wbptr;
    int buffer_size = sysconf(_SC_PAGE_SIZE)
            << whitebox_parameter_get("user_order");
    float fstart = 144e6;
    float fstop = 148e6;
    float fstep = 4e3;

    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE) > 0);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    wbptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, wb.fd, 0);
    assert(wbptr != MAP_FAILED && wbptr);
    tune_accross_band(&wb, fstart, fstop, fstep);
    assert(munmap(wbptr, buffer_size) == 0);
    assert(whitebox_close(&wb) == 0);
}
