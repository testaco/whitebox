#include <stdint.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

#include <whitebox_ioctl.h>

#include "adf4351.h"
#include "cmx991.h"
#include "whitebox_test.h"

#define WHITEBOX_DEV "/dev/whitebox"

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
    ioctl(fd, W_LOCKED, &w);
    assert(!w.locked);
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
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);
    for (i = 0; i < 10; ++i) {
        ret = write(fd, buf, sizeof(uint32_t) * 4);
        assert(ret == sizeof(uint32_t) * 4);
    }
    close(fd);
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

#if 0
int test_blocking_write_underrun(void *data) {
    int fd;
    int ret = 0;
    uint32_t buf[512];
    int i;
    unsigned int status;
    whitebox_args_t w;
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    ioctl(fd, WE_GET, &w);
    w.flags.exciter.interp = 200;
    ioctl(fd, WE_SET, &w);
    assert(write(fd, buf, sizeof(uint32_t) * 512) == sizeof(uint32_t) * 512);
    for (i = 0; write(fd, buf, sizeof(uint32_t)) > 0; ++i) {
        if (i > 100) {
            ret = 1;
            break;
        }
    }
    ioctl(fd, W_STATUS, &status);
    assert(status & W_STATUS_UNDERRUN);
    close(fd);
    return ret;
}

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
    int fd;
    int result;
    fd = open("/sys/module/whitebox/parameters/whitebox_mock_exciter_en", O_WRONLY);
    write(fd, "1");
    close(fd);

    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_blocking_open_close),
        WHITEBOX_TEST(test_blocking_open_busy),
        WHITEBOX_TEST(test_ioctl_reset),
        WHITEBOX_TEST(test_ioctl_not_locked),
        WHITEBOX_TEST(test_ioctl_exciter),
        WHITEBOX_TEST(test_ioctl_cmx991),
        WHITEBOX_TEST(test_ioctl_adf4351),
#if 0
        WHITEBOX_TEST(test_blocking_write),
        WHITEBOX_TEST(test_blocking_write_not_locked),
        WHITEBOX_TEST(test_blocking_write_underrun),
        WHITEBOX_TEST(test_mmap_fail),
        WHITEBOX_TEST(test_mmap_success),
        WHITEBOX_TEST(test_mmap_write_fail),
        WHITEBOX_TEST(test_mmap_write_success),
        WHITEBOX_TEST(test_mmap_write_not_locked),
#endif
        WHITEBOX_TEST(0),
    };
    result = whitebox_test_main(tests, NULL, argc, argv);
    fd = open("/sys/module/whitebox/parameters/whitebox_mock_exciter_en", O_WRONLY);
    write(fd, "0");
    close(fd);
    return result;
}
