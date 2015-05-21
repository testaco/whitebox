#include <stdint.h>
#include "whitebox.h"
#include "whitebox_test.h"
#include "dsp.h"

#define SAMPLE_RATE W_DAC_RATE_HZ / 128

int test_sincos16(void* data) {
    unsigned int i;
    uint32_t phase = 0;
    uint32_t fcw = freq_to_fcw(2000, SAMPLE_RATE);
    for (i = 0; i < 256; ++i) {
        int16_t i, q;
        sincos16(fcw, &phase, &i, &q);
    }
    return 0;
}

int test_cic_shift(void* data) {
    assert(whitebox_cic_shift(128) == 20);
    return 0;
}

int main(int argc, char **argv) {
    dsp_init();
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_sincos16),
        WHITEBOX_TEST(test_cic_shift),
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
