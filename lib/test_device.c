#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include "whitebox.h"
#include "whitebox_test.h"
#include "dsp.h"

#define SAMPLE_RATE W_DAC_RATE_HZ / 128

int test_open_close(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, SAMPLE_RATE) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
    return 0;
}

int test_tx_clear(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, SAMPLE_RATE) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_rx_clear(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_RDONLY, SAMPLE_RATE) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_rx_clear(&wb) == 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
}

int test_ioctl_exciter(void *data) {
    int fd;
    int16_t ic, qc;
    float ig, qg;
    int i;
    int32_t coeffs[WF_COEFF_COUNT], coeffs2[WF_COEFF_COUNT];

    whitebox_t wb;
    whitebox_args_t w;
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE)) > 0);
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

    for (i = 0; i < WF_COEFF_COUNT-1; ++i)
        coeffs[i] = i - (WF_COEFF_COUNT >> 1) + 1;

    assert(whitebox_fir_load_coeffs(&wb, 0, WF_COEFF_COUNT-1, coeffs) == 0);
    assert(whitebox_fir_get_coeffs(&wb, 0, WF_COEFF_COUNT-1, coeffs2) == WF_COEFF_COUNT-1);
    for (i = 0; i < WF_COEFF_COUNT-1; ++i) {
        assert(coeffs[i] == coeffs2[i]);
    }

    whitebox_tx_flags_enable(&wb, WS_FIREN);
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WS_FIREN);
    whitebox_tx_flags_disable(&wb, WS_FIREN);
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(!(w.flags.exciter.state & WS_FIREN));

    assert(whitebox_close(&wb) == 0);
    return 0;
}

int test_ioctl_receiver(void *data) {
    int fd;
    int16_t ic, qc;
    int i;

    whitebox_t wb;
    whitebox_args_t w;
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE)) > 0);
    assert(fd > 0);
    assert(ioctl(fd, WR_GET, &w) == 0);
    w.flags.receiver.decim = 128;
    assert(ioctl(fd, WR_SET, &w) == 0);
    assert(ioctl(fd, WR_GET, &w) == 0);
    assert(w.flags.receiver.decim == 128);

    whitebox_rx_set_correction(&wb, -1, 1);
    whitebox_rx_get_correction(&wb, &ic, &qc);
    assert(ic == -1 && qc == 1);

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
    whitebox_parameter_set("check_plls", 0);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE)) > 0);
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
    whitebox_parameter_set("check_plls", 1);
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
    whitebox_parameter_set("check_plls", 0);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, SAMPLE_RATE)) > 0);
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
    whitebox_parameter_set("check_plls", 1);
    whitebox_parameter_set("auto_tx", 1);
    return 0;
}

int test_tx_overrun_underrun(void *data) {
    whitebox_t wb;
    whitebox_args_t w;
    int i = 200, j;
    int ret;
    int fd;
    uint16_t o, u;
    uint32_t q = 0;

    whitebox_parameter_set("auto_tx", 0);
    whitebox_parameter_set("check_plls", 0);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox",
            O_RDWR | O_NONBLOCK, SAMPLE_RATE)) > 0);
    assert(whitebox_tx(&wb, 144.00e6) == 0);

    whitebox_tx_get_buffer_runs(&wb, &o, &u);
    assert(o == 0 && u == 0);

    while (ret > 0) {
        ret = write(whitebox_fd(&wb), &q, sizeof(uint32_t));
    }
    whitebox_tx_get_buffer_runs(&wb, &o, &u);
    assert(o == 0 && u == 0);

    whitebox_tx_flags_enable(&wb, WES_TXEN);

    whitebox_tx_get_buffer_runs(&wb, &o, &u);
    while (u == 0) {
        whitebox_tx_get_buffer_runs(&wb, &o, &u);
    }

    whitebox_tx_flags_disable(&wb, WES_TXEN);
    assert(whitebox_close(&wb) == 0);
    whitebox_parameter_set("check_plls", 1);
    whitebox_parameter_set("auto_tx", 1);

}

int test_rx_overrun(void *data) {
    whitebox_t wb;
    whitebox_args_t w;
    int i = 200, j;
    int ret;
    int fd;
    uint16_t o, u;
    uint32_t q = 0;

    whitebox_parameter_set("check_plls", 0);
    whitebox_parameter_set("flow_control", 1);
    whitebox_init(&wb);
    assert((fd = whitebox_open(&wb, "/dev/whitebox",
            O_RDWR, SAMPLE_RATE)) > 0);
    assert(whitebox_rx(&wb, 144.00e6) == 0);

    assert(read(whitebox_fd(&wb), &q, sizeof(uint32_t)) > 0);

    sleep(2);

    assert(read(whitebox_fd(&wb), &q, sizeof(uint32_t)) < 0);

    assert(whitebox_close(&wb) == 0);
    whitebox_parameter_set("check_plls", 1);
}

#define COUNT 1024 
int test_tx_halt(void* data) {
    whitebox_t wb;
    whitebox_args_t w;
    int j, k;
    int ret;
    int fd;
    int last_count = 0;
    float sample_rate = SAMPLE_RATE;
    uint32_t fcw = freq_to_fcw(1.7e3, sample_rate);
    uint32_t phase = 0;
    void *wbptr;
    int buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    whitebox_init(&wb);
    whitebox_parameter_set("flow_control", 1);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    wbptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(wbptr != MAP_FAILED && wbptr);

    assert(whitebox_tx_set_latency(&wb, 20) == 0);
    assert(whitebox_tx_get_latency(&wb) == 20);

    for (k = 0; k < 10; ++k) {
        assert(whitebox_tx(&wb, 145.00e6) == 0);
        while (!whitebox_plls_locked(&wb)) {
            continue;
        }
        assert(whitebox_plls_locked(&wb));
        last_count = 0;
        for (j = 0; j < 10;) {
            unsigned long dest, count;
            count = ioctl(fd, W_MMAP_WRITE, &dest) >> 2;
            if (count <= 0) {
                continue;
            } else {
                count = count < COUNT ? count : COUNT;
                if (rand() & 1)
                    count -= 16;
                //phase = sincos16c(count, fcw, phase, (uint32_t*)dest);
                assert(whitebox_plls_locked(&wb));
                ret = write(whitebox_fd(&wb), 0, count << 2);
                if (ret != count << 2) {
                    whitebox_debug_to_file(&wb, stdout);
                    printf("locked: CMX991 %c     ADF4351 %c\n",
                        ioctl(wb.fd, WC_LOCKED) ? 'Y' : 'N',
                        ioctl(wb.fd, WA_LOCKED) ? 'Y' : 'N');
                    perror("write: ");
                }
                assert(ret == count << 2);
                last_count += count;
                //whitebox_debug_to_file(&wb, stdout);
                j++;
            }
        }
        assert(fsync(fd) == 0);
        whitebox_tx_standby(&wb);
        ioctl(wb.fd, WE_GET, &w);
        assert(last_count == w.flags.exciter.debug);
    }
    whitebox_parameter_set("flow_control", 1);

    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(!(w.flags.exciter.state & WES_TXEN));
    assert(munmap(wbptr, buffer_size) == 0);
    assert(whitebox_close(&wb) == 0);
}

int test_rx_halt(void* data) {
    whitebox_t wb;
    whitebox_args_t w;
    int j, k;
    int ret;
    int fd;
    int last_count = 0;
    float sample_rate = SAMPLE_RATE;
    uint32_t fcw = freq_to_fcw(1.7e3, sample_rate);
    uint32_t phase = 0;
    void *wbptr;
    static uint32_t buf[COUNT];
    int buffer_size = sysconf(_SC_PAGE_SIZE) << whitebox_parameter_get("user_order");
    whitebox_init(&wb);
    whitebox_parameter_set("flow_control", 1);
    assert((fd = whitebox_open(&wb, "/dev/whitebox", O_RDWR, sample_rate)) > 0);
    wbptr = mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(wbptr != MAP_FAILED && wbptr);

    assert(whitebox_rx_set_latency(&wb, 20) == 0);
    assert(whitebox_rx_get_latency(&wb) == 20);

    for (k = 0; k < 8; ++k) {
        assert(whitebox_rx(&wb, 145.00e6) == 0);
        while (!whitebox_plls_locked(&wb)) {
            continue;
        }
        assert(whitebox_plls_locked(&wb));
        last_count = 0;
        for (j = 0; j < 10;) {
            unsigned long dest;
            long count;
            count = ioctl(fd, W_MMAP_READ, &dest);// >> 2;
            assert(count >= 0);
            count >>= 2;
            if (count == 0) {
                continue;
            } else {
                count = count < COUNT ? count : COUNT;
                if (rand() & 1)
                    count -= 16;
                //phase = sincos16c(count, fcw, phase, (uint32_t*)dest);
                assert(whitebox_plls_locked(&wb));
                ret = read(whitebox_fd(&wb), (void*)&buf, count << 2);
                if (ret != count << 2) {
                    whitebox_debug_to_file(&wb, stdout);
                    printf("locked: CMX991 %c     ADF4351 %c\n",
                        ioctl(wb.fd, WC_LOCKED) ? 'Y' : 'N',
                        ioctl(wb.fd, WA_LOCKED) ? 'Y' : 'N');
                    perror("read: ");
                }
                assert(ret == count << 2);
                last_count += count;
                //whitebox_debug_to_file(&wb, stdout);
                j++;
            }
        }
        assert(fsync(fd) == 0);
        whitebox_rx_standby(&wb);
        //ioctl(wb.fd, WR_GET, &w);
        //assert(last_count == w.flags.receiver.debug);
    }
    whitebox_parameter_set("flow_control", 1);

    assert(ioctl(fd, WR_GET, &w) == 0);
    assert(!(w.flags.receiver.state & WRS_RXEN));
    assert(munmap(wbptr, buffer_size) == 0);
    assert(whitebox_close(&wb) == 0);
}

int main(int argc, char **argv) {
    whitebox_parameter_set("mock_en", 1);
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_open_close),
        WHITEBOX_TEST(test_tx_clear),
        WHITEBOX_TEST(test_rx_clear),
        WHITEBOX_TEST(test_ioctl_exciter),
        WHITEBOX_TEST(test_ioctl_receiver),
#if 0
        WHITEBOX_TEST(test_tx_overrun_underrun),
        WHITEBOX_TEST(test_tx_halt),
        WHITEBOX_TEST(test_rx_overrun),
        WHITEBOX_TEST(test_rx_halt),
        WHITEBOX_TEST(test_tx_fifo),
        WHITEBOX_TEST(test_tx_fifo_dma),
#endif
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
