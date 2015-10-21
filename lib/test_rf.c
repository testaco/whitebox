#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include "whitebox.h"
#include "whitebox_test.h"
#include "dsp.h"

#define SAMPLE_RATE W_DAC_RATE_HZ / 128

float diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1e9+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return (temp.tv_sec * 1e9 + temp.tv_nsec) / 1e9 * 1e3;
}

int _test_tx_pll(float freq) {
    //struct timespec tx_start, tx_ready;
    whitebox_t wb;
    whitebox_init(&wb);
    //clock_gettime(CLOCK_MONOTONIC, &tx_start);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, SAMPLE_RATE) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, freq) == 0);
    whitebox_plls_locked(&wb);
    assert(whitebox_plls_locked(&wb));
    //clock_gettime(CLOCK_MONOTONIC, &tx_ready);
    //printf("tx start to ready: %f\n", diff(tx_start, tx_ready));
    assert(whitebox_close(&wb) == 0);
}

int test_tx_50_pll(void* data) {
    _test_tx_pll(50e6);
    _test_tx_pll(144e6);
    _test_tx_pll(222e6);
    return 0;
}

int test_tx_144_pll(void* data) {
    return _test_tx_pll(144e6);
}

int test_tx_222_pll(void* data) {
    return _test_tx_pll(222e6);
}

int test_tx_420_pll(void* data) {
    return _test_tx_pll(420e6);
}

int test_tx_902_pll(void* data) {
    return _test_tx_pll(902e6);
}

int _test_rx_pll(float freq) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_RDONLY, SAMPLE_RATE) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_rx_clear(&wb) == 0);
    assert(whitebox_rx(&wb, freq) == 0);
    whitebox_plls_locked(&wb);
    assert(whitebox_plls_locked(&wb));
    //assert(ioctl(wb.fd, W_LOCKED));
    assert(whitebox_close(&wb) == 0);
}

int test_rx_50_pll(void* data) {
    return _test_rx_pll(50e6);
}

int test_rx_144_pll(void* data) {
    return _test_rx_pll(144e6);
}

int test_rx_222_pll(void* data) {
    return _test_rx_pll(222e6);
}

int test_rx_420_pll(void* data) {
    return _test_rx_pll(420e6);
}

int test_rx_902_pll(void* data) {
    return _test_rx_pll(902e6);
}

int main(int argc, char **argv) {
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_tx_50_pll),
        WHITEBOX_TEST(test_tx_144_pll),
        WHITEBOX_TEST(test_tx_222_pll),
        WHITEBOX_TEST(test_tx_420_pll),
        WHITEBOX_TEST(test_tx_902_pll),
        WHITEBOX_TEST(test_rx_50_pll),
        WHITEBOX_TEST(test_rx_144_pll),
        WHITEBOX_TEST(test_rx_222_pll),
        WHITEBOX_TEST(test_rx_420_pll),
        WHITEBOX_TEST(test_rx_902_pll),
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
