#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "adf4351.h"

#include "whitebox_test.h"

int test_adf4351_pack_unpack(void *data) {
    adf4351_t adf4351;
    adf4351_load(&adf4351, 0x00180005);
    adf4351_load(&adf4351, 0x00CD01FC);
    adf4351_load(&adf4351, 0x000004B3);
    adf4351_load(&adf4351, 0x00004EC2);
    adf4351_load(&adf4351, 0x00000069);
    adf4351_load(&adf4351, 0x003C8058);

    assert(0x00180005 == adf4351_pack(&adf4351, 5));
    assert(0x00CD01FC == adf4351_pack(&adf4351, 4));
    assert(0x000004B3 == adf4351_pack(&adf4351, 3));
    assert(0x00004EC2 == adf4351_pack(&adf4351, 2));
    assert(0x00000069 == adf4351_pack(&adf4351, 1));
    assert(0x003C8058 == adf4351_pack(&adf4351, 0));

    assert(abs(198e6 - adf4351_actual_frequency(&adf4351)) < 1e3);
    return 0;
}

int test_adf4351_compute_frequency(void *data) {
    adf4351_t adf4351;
    adf4351_load(&adf4351, 0x00180005);
    adf4351_load(&adf4351, 0x00CD01FC);
    adf4351_load(&adf4351, 0x000004B3);
    adf4351_load(&adf4351, 0x00004EC2);
    adf4351_load(&adf4351, 0x00000069);
    adf4351_load(&adf4351, 0x003C8058);
    assert(abs(198e6 - adf4351_actual_frequency(&adf4351)) < 1e3);
    return 0;
}

int test_adf4351_tune(void *data) {
    adf4351_t adf4351;
    adf4351_pll_enable(&adf4351, 26e6, 100e3, 198e6);
    assert(fabs(198e6 - adf4351_actual_frequency(&adf4351)) < 1e3);
    return 0;
}

int main(int argc, char **argv) {
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_adf4351_pack_unpack),
        WHITEBOX_TEST(test_adf4351_compute_frequency),
        WHITEBOX_TEST(test_adf4351_tune),
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
