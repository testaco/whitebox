#include <stdint.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

#include <whitebox_ioctl.h>

#include "adf4351.h"
#include "cmx991.h"
#include "whitebox_test.h"

#define WHITEBOX_DEV "/dev/whitebox"

int whitebox_parameter_set(const char *param, int value)
{
    char name[512];
    char final_value[128];
    int fd;
    snprintf(name, 512, "/sys/module/whitebox/parameters/whitebox_%s", param);
    snprintf(final_value, 128, "%d\n", value);
    fd = open(name, O_WRONLY);
    if (fd < 0)
        return fd;
    if (write(fd, final_value, strlen(final_value)+1) < 0) {
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

int whitebox_parameter_get(const char *param)
{
    char name[512];
    char final_value[128];
    int fd;
    snprintf(name, 512, "/sys/module/whitebox/parameters/whitebox_%s", param);
    fd = open(name, O_RDONLY);
    if (fd < 0)
        return fd;
    if (read(fd, &final_value, 127) < 0) {
        close(fd);
        return 1;
    }
    close(fd);
    return atoi(final_value);
}

int test_blocking_open_close(void* data) {
    int fd;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    close(fd);
    return 0;
}

int test_blocking_open_busy(void* data) {
    int fd;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    assert(open(WHITEBOX_DEV, O_WRONLY) < 0);
    close(fd);
    return 0;
}

int test_ioctl_reset(void* data) {
    int fd;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, W_RESET);
    close(fd);
    return 0;
}

int test_ioctl_not_locked(void *data) {
    int fd;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    assert(!ioctl(fd, W_LOCKED));
    close(fd);
    return 0;
}

int test_ioctl_exciter(void *data) {
    int fd;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_CLEAR);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 100;
    w.flags.exciter.fcw = 32;
    ioctl(fd, WE_SET, &w);
    ioctl(fd, WE_GET, &w);
    assert(w.flags.exciter.interp == 100);
    assert(w.flags.exciter.fcw == 32);
    close(fd);
    return 0;
}

int test_ioctl_cmx991(void *data) {
    int fd;
    whitebox_args_t w;
    cmx991_t cmx991;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, W_RESET);
    ioctl(fd, WC_GET, &w);
    cmx991_ioctl_get(&cmx991, &w);
    cmx991.tx_mix_pwr = TX_MIX_PWR_POWER_UP;
    cmx991_ioctl_set(&cmx991, &w);
    ioctl(fd, WC_SET, &w);
    ioctl(fd, WC_GET, &w);
    cmx991_ioctl_get(&cmx991, &w);
    assert(cmx991.tx_mix_pwr == TX_MIX_PWR_POWER_UP);
    close(fd);
    return 0;
}

int test_ioctl_adf4351(void *data) {
    int fd;
    whitebox_args_t w;
    adf4351_t adf4351;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, W_RESET);
    ioctl(fd, WC_GET, &w);
    adf4351_ioctl_get(&adf4351, &w);
    adf4351.frac_12_bit = 100;
    adf4351_ioctl_set(&adf4351, &w);
    ioctl(fd, WC_SET, &w);
    ioctl(fd, WC_GET, &w);
    adf4351_ioctl_get(&adf4351, &w);
    assert(adf4351.frac_12_bit == 100);
    close(fd);
    return 0;
}

int test_blocking_write(void *data) {
    int fd;
    int ret;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);
    for (i = 0; i < 10; ++i) {
        ret = write(fd, buf, sizeof(uint32_t) * 4);
        assert(ret == sizeof(uint32_t) * 4);
    }
    assert(fsync(fd) == 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

#define HUGE 1024
int test_blocking_xfer_huge(void *data) {
    int fd;
    int ret;
    uint32_t *buf, *buf2;
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    buf = malloc(sizeof(uint32_t) * HUGE);
    buf2 = malloc(sizeof(uint32_t) * HUGE);
    assert(buf && buf2);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);
    for (i = 0; i < 1000; ++i) {
        for (i = 0; i < HUGE; ++i) {
            buf[i] = rand();
        }
        ret = write(fd, buf, sizeof(uint32_t) * HUGE);
        assert(ret == sizeof(uint32_t) * HUGE);

        assert(fsync(fd) == 0);

        ret = read(fd, buf2, sizeof(uint32_t) * HUGE);
        assert(ret == sizeof(uint32_t) * HUGE);
        assert(memcmp(buf, buf2, sizeof(uint32_t) * HUGE) == 0);

        assert(fsync(fd) == 0);
    }
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    free(buf);
    free(buf2);
    return 0;
}

int test_blocking_write_not_locked(void *data) {
    int fd;
    int ret;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    assert(write(fd, buf, sizeof(uint32_t) * 4) < 0);
    close(fd);
    return 0;
}

int test_blocking_write_underrun(void *data) {
    int fd;
    int ret = 0;
    uint32_t buf[512];
    int i;
    unsigned int status;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);

    assert(write(fd, buf, sizeof(uint32_t) * 512) ==
            sizeof(uint32_t) *512);
    w.mock_command = WMC_CAUSE_UNDERRUN;
    ioctl(fd, WM_CMD, &w);
    assert(write(fd, buf, sizeof(uint32_t) * 512) < 0);
    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return ret;
}

int test_blocking_xfer(void *data) {
    int fd;
    int ret;
    uint32_t buf[4];
    uint32_t buf2[4];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 4; ++i)
        buf[i] = i;

    ret = write(fd, buf, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    assert(fsync(fd) == 0);

    ret = read(fd, buf2, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    assert(fsync(fd) == 0);

    assert(memcmp(buf, buf2, sizeof(uint32_t) * 4) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_xfer2(void *data) {
    int fd;
    int ret;
    uint32_t buf_in1[4], buf_in2[4];
    uint32_t buf_out[8];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 4; ++i) {
        buf_in1[i] = rand();
        buf_in2[i] = rand();
    }

    ret = write(fd, buf_in1, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    ret = write(fd, buf_in2, sizeof(uint32_t) * 4);
    assert(ret == sizeof(uint32_t) * 4);
    assert(fsync(fd) == 0);

    ret = read(fd, buf_out, sizeof(uint32_t) * 8);
    assert(ret == sizeof(uint32_t) * 8);
    assert(fsync(fd) == 0);

    assert(memcmp(buf_in1, buf_out, sizeof(uint32_t) * 4) == 0);
    assert(memcmp(buf_in2, buf_out + 4, sizeof(uint32_t) * 4) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_xfer3(void *data) {
    int fd;
    int ret;
    uint32_t buf_in1[128], buf_in2[128];
    uint32_t buf_out[256];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 128; ++i) {
        buf_in1[i] = rand();
        buf_in2[i] = rand();
    }

    ret = write(fd, buf_in1, sizeof(uint32_t) * 128);
    assert(ret == sizeof(uint32_t) * 128);
    ret = write(fd, buf_in2, sizeof(uint32_t) * 128);
    assert(ret == sizeof(uint32_t) * 128);
    assert(fsync(fd) == 0);

    ret = read(fd, buf_out, sizeof(uint32_t) * 256);
    assert(ret == sizeof(uint32_t) * 256);
    assert(fsync(fd) == 0);

    assert(memcmp(buf_in1, buf_out, sizeof(uint32_t) * 128) == 0);
    assert(memcmp(buf_in2, buf_out + 128, sizeof(uint32_t) * 128) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_blocking_xfer4(void *data) {
    int fd;
    int ret;
    uint32_t buf_in1[200], buf_in2[200];
    uint32_t buf_out[400];
    int i;
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    for (i = 0; i < 200; ++i) {
        buf_in1[i] = rand();
        buf_in2[i] = rand();
    }

    ret = write(fd, buf_in1, sizeof(uint32_t) * 200);
    assert(ret == sizeof(uint32_t) * 200);
    ret = write(fd, buf_in2, sizeof(uint32_t) * 200);
    assert(ret == sizeof(uint32_t) * 200);
    assert(fsync(fd) == 0);

    ret = read(fd, buf_out, sizeof(uint32_t) * 400);
    assert(ret == sizeof(uint32_t) * 400);
    assert(fsync(fd) == 0);

    assert(memcmp(buf_in1, buf_out, sizeof(uint32_t) * 200) == 0);
    assert(memcmp(buf_in2, buf_out + 200, sizeof(uint32_t) * 200) == 0);

    close(fd);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

int test_tx_fifo(void *data) {
    int fd;
    int ret;
    uint32_t sample = 0xdeadbeef;
    int i = 0;
    int quantum = whitebox_parameter_get("exciter_quantum");
    whitebox_args_t w;
    assert(whitebox_parameter_set("check_plls", 0) == 0);
    assert(whitebox_parameter_set("loopen", 0) == 0);
    fd = open(WHITEBOX_DEV, O_RDWR);
    assert(fd > 0);

    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WES_SPACE);
    assert(w.flags.exciter.state & WES_AEMPTY);
    assert(!(w.flags.exciter.state & WES_DATA));
    assert(!(w.flags.exciter.state & WES_AFULL));

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));

    while (++i < (quantum >> 2) - 1) {
        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
        assert(ioctl(fd, WE_GET, &w) == 0);
        assert(w.flags.exciter.state & WES_SPACE);
        assert(w.flags.exciter.state & WES_AEMPTY);
        assert(w.flags.exciter.state & WES_DATA);
        assert(!(w.flags.exciter.state & WES_AFULL));
    }

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WES_SPACE);
    assert(!(w.flags.exciter.state & WES_AEMPTY));
    assert(w.flags.exciter.state & WES_DATA);
    assert(!(w.flags.exciter.state & WES_AFULL));

    while (++i < WE_FIFO_SIZE - (quantum >> 2) - 1) {
        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
        assert(ioctl(fd, WE_GET, &w) == 0);
        assert(w.flags.exciter.state & WES_SPACE);
        assert(!(w.flags.exciter.state & WES_AEMPTY));
        assert(w.flags.exciter.state & WES_DATA);
        assert(!(w.flags.exciter.state & WES_AFULL));
    }

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(w.flags.exciter.state & WES_SPACE);
    assert(!(w.flags.exciter.state & WES_AEMPTY));
    assert(w.flags.exciter.state & WES_DATA);
    assert(w.flags.exciter.state & WES_AFULL);

    while (++i < WE_FIFO_SIZE - 2) {
        assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
        assert(ioctl(fd, WE_GET, &w) == 0);
        assert(w.flags.exciter.state & WES_SPACE);
        assert(!(w.flags.exciter.state & WES_AEMPTY));
        assert(w.flags.exciter.state & WES_DATA);
        assert(w.flags.exciter.state & WES_AFULL);
    }

    assert(write(fd, &sample, sizeof(uint32_t)) == sizeof(uint32_t));
    assert(ioctl(fd, WE_GET, &w) == 0);
    assert(!(w.flags.exciter.state & WES_SPACE));
    assert(!(w.flags.exciter.state & WES_AEMPTY));
    assert(w.flags.exciter.state & WES_DATA);
    assert(w.flags.exciter.state & WES_AFULL);

    close(fd);

    assert(whitebox_parameter_set("loopen", 1) == 0);
    assert(whitebox_parameter_set("check_plls", 1) == 0);
    return 0;
}

#if 0
int test_mmap_success(void *data) {
    int fd;
    int ret;
    int rbsize;
    void* rbptr;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_RDWR | O_NONBLOCK);
    assert(fd > 0);
    ioctl(fd, WE_GET_RB_INFO, &w);
    rbsize = w.rb_info.size;
    rbptr = mmap(0, rbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr != MAP_FAILED && rbptr);
    assert(munmap(rbptr, rbsize) == 0);
    close(fd);
    return 0;
}

int test_mmap_fail(void *data) {
    int fd;
    int ret;
    int rbsize;
    void* rbptr;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_RDWR | O_NONBLOCK);
    assert(fd > 0);
    ioctl(fd, WE_GET_RB_INFO, &w);
    rbsize = w.rb_info.size;
    // bad offset
    rbptr = mmap(0, rbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 10);
    assert(rbptr == MAP_FAILED);
    // bad size
    rbptr = mmap(0, 0, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr == MAP_FAILED);
    close(fd);
    return 0;
}

int test_mmap_write_fail(void *data) {
    int fd;
    int ret;
    int rbsize;
    void* rbptr;
    whitebox_args_t w;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };

    fd = open(WHITEBOX_DEV, O_RDWR | O_NONBLOCK);
    assert(fd > 0);
    ioctl(fd, WE_GET_RB_INFO, &w);
    rbsize = w.rb_info.size;
    rbptr = mmap(0, rbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr > 0);
    ret = write(fd, buf, sizeof(uint32_t) * 4);
    assert(ret < 0);
    assert(munmap(rbptr, rbsize) == 0);
    close(fd);
    return 0;
}

int test_mmap_write_success(void *data) {
    int fd;
    int ret;
    int rbsize;
    void* rbptr;
    whitebox_args_t w;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    fd = open(WHITEBOX_DEV, O_RDWR | O_NONBLOCK);
    assert(fd > 0);
    ioctl(fd, WE_GET_RB_INFO, &w);
    rbsize = w.rb_info.size;
    rbptr = mmap(0, rbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr > 0);
    for (i = 0; i < 10; ++i) {
        ioctl(fd, WE_GET_RB_PAGE, &w);
        memcpy(rbptr + w.rb_info.offset, buf, sizeof(uint32_t) * 4);
        ret = write(fd, 0, sizeof(uint32_t) * 4);
        assert(ret == sizeof(uint32_t) * 4);
    }
    assert(munmap(rbptr, rbsize) == 0);
    close(fd);
    return 0;
}

int test_mmap_write_not_locked(void *data) {
    int fd;
    int ret;
    int rbsize;
    void* rbptr;
    whitebox_args_t w;
    uint32_t buf[] = { 0x00, 0x01, 0x02, 0x03 };
    int i;
    fd = open(WHITEBOX_DEV, O_RDWR | O_NONBLOCK);
    assert(fd > 0);
    ioctl(fd, WE_GET_RB_INFO, &w);
    rbsize = w.rb_info.size;
    rbptr = mmap(0, rbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(rbptr > 0);
    ioctl(fd, WE_GET_RB_PAGE, &w);
    memcpy(rbptr + w.rb_info.offset, buf, sizeof(uint32_t) * 4);
    ret = write(fd, 0, sizeof(uint32_t) * 4);
    assert(ret < 0);
    assert(munmap(rbptr, rbsize) == 0);
    close(fd);
    return 0;
}
#endif

int main(int argc, char **argv) {
    int result;

    whitebox_parameter_set("mock_en", 1);
    whitebox_parameter_set("loopen", 1);

    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_blocking_open_close),
        WHITEBOX_TEST(test_blocking_open_busy),
        WHITEBOX_TEST(test_ioctl_reset),
        WHITEBOX_TEST(test_ioctl_not_locked),
        WHITEBOX_TEST(test_ioctl_exciter),
        WHITEBOX_TEST(test_ioctl_cmx991),
        WHITEBOX_TEST(test_ioctl_adf4351),
        WHITEBOX_TEST(test_blocking_write),
        WHITEBOX_TEST(test_blocking_write_not_locked),
        WHITEBOX_TEST(test_blocking_write_underrun),
        WHITEBOX_TEST(test_blocking_xfer),
        WHITEBOX_TEST(test_blocking_xfer2),
        WHITEBOX_TEST(test_blocking_xfer3),
        WHITEBOX_TEST(test_blocking_xfer4),
        WHITEBOX_TEST(test_tx_fifo),
        WHITEBOX_TEST(test_blocking_xfer_huge),
#if 0
        WHITEBOX_TEST(test_mmap_fail),
        WHITEBOX_TEST(test_mmap_success),
        WHITEBOX_TEST(test_mmap_write_fail),
        WHITEBOX_TEST(test_mmap_write_success),
        WHITEBOX_TEST(test_mmap_write_not_locked),
#endif
        WHITEBOX_TEST(0),
    };
    result = whitebox_test_main(tests, NULL, argc, argv);
    whitebox_parameter_set("mock_en", 0);
    whitebox_parameter_set("loopen", 0);
    return result;
}
