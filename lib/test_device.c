#include "whitebox.h"
#include "whitebox_test.h"

int test_open_close(void* data) {
    whitebox_t wb;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 1e6) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(!whitebox_plls_locked(&wb));
    assert(whitebox_close(&wb) == 0);
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

int test_tx_write(void* data) {
    whitebox_t wb;
    uint32_t buf[1023];
    int i = 200, j;
    int ret;
    whitebox_init(&wb);
    assert(whitebox_open(&wb, "/dev/whitebox", O_WRONLY, 50e3) > 0);
    assert(whitebox_reset(&wb) == 0);
    assert(whitebox_tx_clear(&wb) == 0);
    assert(whitebox_tx(&wb, 144.00e6) == 0);

    for (j = 0; j < 10; ++j) {
        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);

        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);

        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);

        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);

        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);

        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);

        ret = write(whitebox_fd(&wb), buf, sizeof(uint32_t) * i);
        assert(ret == sizeof(uint32_t) * i);
    }

    assert(whitebox_status(&wb) == W_STATUS_READY);
    assert(whitebox_close(&wb) == 0);
}

int main(int argc, char **argv) {
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_open_close),
        WHITEBOX_TEST(test_tx_clear),
        WHITEBOX_TEST(test_tx_50_pll_fails),
        WHITEBOX_TEST(test_tx_144_pll),
        WHITEBOX_TEST(test_tx_222_pll),
        WHITEBOX_TEST(test_tx_420_pll),
        WHITEBOX_TEST(test_tx_902_pll),
        WHITEBOX_TEST(test_tx_write),
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
