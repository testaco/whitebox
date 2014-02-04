#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "whitebox.h"
#include "whitebox_test.h"
#include "dsp.h"

int test_open_close(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
    return 0;
}

int test_tx_clear(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_tx_50_pll_fails(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, 50.00e6) != 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_tx_144_pll(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, 144.00e6) == 0);
    assert(whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_tx_222_pll(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, 222.00e6) == 0);
    assert(whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_tx_420_pll(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, 420.00e6) == 0);
    assert(whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_tx_902_pll(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, 902.00e6) == 0);
    assert(whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_ioctl_exciter(void *data) {
    int fd;
    int16_t ic, qc;
    float ig, qg;
    whitebox_t wb;
    whitebox_args_t w;
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, 1e6)) > 0);
    assert(fd > 0);
    assert(ioctl(fd, WE_GET, &w) == 0);
    w.flags.exciter.interp = 100;
    w.flags.exciter.fcw = 32;
    assert(ioctl(fd, WE_SET, &w) == 0);
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.interp == 100);
    assert(w.flags.exciter.fcw == 32);

    whitebox_tx_set_correction(&wb, -1, 1);
    whitebox_tx_get_correction(&wb, &ic, &qc);
    assert(ic == -1 && qc == 1);

    assert(whitebox_tx_set_gain(&wb, 0.75, 1.25) == 0);
    whitebox_tx_get_gain(&wb, &ig, &qg);
    assert(ig == 0.75 && qg == 1.25);

    assert(whitebox_close(&wb) == 0);
    return 0;
}

int test_tx_fifo(void *data) {
    int fd;
    int ret;
    uint32_t sample = 0xdeadbeef;
    int i = 0;
    whitebox_t wb;
    int quantum = whitebox_parameter_get("exciter_quantum");
    whitebox_args_t w;
    uint16_t aeval, afval;

    whitebox_parameter_set("auto_tx", 0);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, 10e3)) > 0);
    whitebox_tx_get_buffer_threshold(&wb, &aeval, &afval);
    assert(whitebox_tx(&wb, 144.00e6) == 0);

    for (i = 0; i < 1; ++i) {
        assert(ioctl(fd, WE_GET, &w) == 0);

        //printf("availalbe=%d\n", w.flags.exciter.available);
        //assert(w.flags.exciter.available >> 2 == WE_FIFO_SIZE - i);

        if (i == WE_FIFO_SIZE)
            assert(!(w.flags.exciter.state & WES_SPACE));
        else
            assert(w.flags.exciter.state & WES_SPACE);

        if (i == 0)
            assert(!(w.flags.exciter.state & WES_DATA));
        else
            assert(w.flags.exciter.state & WES_DATA);

        if (i < afval)
            assert(!(w.flags.exciter.state & WES_AFULL));
        else
            assert(w.flags.exciter.state & WES_AFULL);

        if (i > aeval)
            assert(!(w.flags.exciter.state & WES_AEMPTY));
        else
            assert(w.flags.exciter.state & WES_AEMPTY);

        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    }

    whitebox_close(&wb);
    whitebox_parameter_set("auto_tx", 1);
    return 0;
}

int test_tx_fifo_dma(void *data) {
    int fd;
    int ret;
    uint32_t samples[] = { 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef };
    int i = 0;
    whitebox_t wb;
    int quantum = whitebox_parameter_get("exciter_quantum");
    whitebox_args_t w;
    uint16_t aeval, afval;

    whitebox_parameter_set("auto_tx", 0);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, 10e3)) > 0);
    whitebox_tx_get_buffer_threshold(&wb, &aeval, &afval);
    assert(whitebox_tx(&wb, 144.00e6) == 0);

    for (i = 0; i < WE_FIFO_SIZE; i += 4) {
        assert(ioctl(fd, WE_GET, &w) == 0);

        //assert(w.flags.exciter.available >> 2 == WE_FIFO_SIZE - i);

        if (i == WE_FIFO_SIZE)
            assert(!(w.flags.exciter.state & WES_SPACE));
        else
            assert(w.flags.exciter.state & WES_SPACE);

        if (i == 0)
            assert(!(w.flags.exciter.state & WES_DATA));
        else
            assert(w.flags.exciter.state & WES_DATA);

        if (i < afval)
            assert(!(w.flags.exciter.state & WES_AFULL));
        else
            assert(w.flags.exciter.state & WES_AFULL);

        if (i > aeval)
            assert(!(w.flags.exciter.state & WES_AEMPTY));
        else
            assert(w.flags.exciter.state & WES_AEMPTY);

        assert(write(fd, &samples, 4 * sizeof(uint32_t)) == 4 * sizeof(uint32_t));
    }

    whitebox_close(&wb);
    whitebox_parameter_set("auto_tx", 1);
    return 0;
}

int test_tx_overrun_underrun(void *data) {
    whitebox_t wb;
    whitebox_args_t w;
    uint32_t buf[1023];
    int i = 200, j;
    int ret;
    int fd;
    uint16_t o, u;

    whitebox_parameter_set("auto_tx", 0);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox",
            O_RDWR | O_NONBLOCK, 50e3)) > 0);
    assert(whitebox_tx(&wb, 144.00e6) == 0);

    whitebox_tx_get_buffer_runs(&wb, &o, &u);
    assert(o == 0 && u == 0);

    while (ret > 0) {
        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
    }
    whitebox_tx_get_buffer_runs(&wb, &o, &u);
    assert(o == 0 && u == 0);

    whitebox_tx_flags_enable(&wb, WES_TXEN);

    whitebox_tx_get_buffer_runs(&wb, &o, &u);
    while (u == 0) {
        whitebox_tx_get_buffer_runs(&wb, &o, &u);
    }

    assert(whitebox_close(&wb) == 0);
    whitebox_parameter_set("auto_tx", 1);
}

#define COUNT 2048
#define SAMP_RATE 500e3
int test_tx_halt(void* data) {
    whitebox_t wb;
    whitebox_args_t w;
    int j;
    int ret;
    int fd;
    int runs;
    float sample_rate = SAMP_RATE;
    uint32_t fcw = freq_to_fcw(1.7e3, sample_rate);
    void *wbptr;
    int buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    wbptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(wbptr != MAP_FAILED && wbptr);
    assert(whitebox_tx(&wb, 144.00e6) == 0);

    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(!(w.flags.exciter.state & WES_TXEN));
    runs = w.flags.exciter.runs;

    whitebox_debug_to_file(&wb, stdout);
    for (j = 0; j < 5000; ++j) {
        unsigned long dest, count;
        count = ioctl(fd, W_MMAP_WRITE, &dest);
        if (count <= 0) {
            printf("busy\n");
        } else {
            count = count < COUNT ? count : COUNT;
            //accum32(count >> 2, fcw, 0, (uint32_t*)dest);
            //sincos16c(count >> 2, (uint32_t*)dest, (uint32_t*)dest);
            ret = write(whitebox_fd(&wb), 0, count);
            if (ret != count) {
                whitebox_debug_to_file(&wb, stdout);
                perror("write: ");
            }
            assert(ret == count);
        }
        if (j % 1000 == 0)
            whitebox_debug_to_file(&wb, stdout);
    }

    assert(fsync(fd) == 0);
    whitebox_debug_to_file(&wb, stdout);

    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(!(w.flags.exciter.state & WES_TXEN));
    assert(w.flags.exciter.runs == runs);
    assert(munmap(wbptr, buffer_size) == 0);
    assert(whitebox_close(&wb) == 0);
}

int main(int argc, char **argv) {
    whitebox_parameter_set("mock_en", 0);
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_tx_fifo_dma),
        WHITEBOX_TEST(test_tx_fifo),
        WHITEBOX_TEST(test_open_close),
        WHITEBOX_TEST(test_tx_clear),
        WHITEBOX_TEST(test_tx_50_pll_fails),
        WHITEBOX_TEST(test_tx_144_pll),
        WHITEBOX_TEST(test_tx_222_pll),
        WHITEBOX_TEST(test_tx_420_pll),
        WHITEBOX_TEST(test_tx_902_pll),
        WHITEBOX_TEST(test_ioctl_exciter),
        WHITEBOX_TEST(test_tx_overrun_underrun),
        WHITEBOX_TEST(test_tx_halt),
#if 0
#endif
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
