#include <assert.h>
#include <math.h>

#include "cmx991.h"

void test_cmx991_unpack_state() {
    printf("test_cmx991_unpack_state... ");
    cmx991_t cmx991;
    cmx991_load(&cmx991, 0x11, 0x8f);
    cmx991_load(&cmx991, 0x14, 0x50);
    cmx991_load(&cmx991, 0x15, 0x14);
    cmx991_load(&cmx991, 0x16, 0x00);
    cmx991_load(&cmx991, 0x20, 0xc0);
    cmx991_load(&cmx991, 0x21, 0xa0);
    cmx991_load(&cmx991, 0x22, 0x08);
    cmx991_load(&cmx991, 0x23, 0x07);

    assert(192 == cmx991_get_m(&cmx991));
    assert(1800 == cmx991_get_n(&cmx991));
    // Assert less than 1 Hz offset from expected value
    printf("actual %f\n", cmx991_pll_actual_frequency(&cmx991, 19.2e6));
    printf("diff   %f\n", fabs(180.0e6 - cmx991_pll_actual_frequency(&cmx991, 19.2e6)));
    assert(fabs(180.0e6 - cmx991_pll_actual_frequency(&cmx991, 19.2e6)) < 1.0);

    printf("passed\n");
}

void test_cmx991_pack_unpack() {
    printf("test_cmx991_pack_unpack... ");
    cmx991_t cmx991;
    cmx991_load(&cmx991, 0x11, 0x8f);
    cmx991_load(&cmx991, 0x14, 0x50);
    cmx991_load(&cmx991, 0x15, 0x14);
    cmx991_load(&cmx991, 0x16, 0x00);
    cmx991_load(&cmx991, 0x20, 0xc0);
    cmx991_load(&cmx991, 0x21, 0xa0);
    cmx991_load(&cmx991, 0x22, 0x08);
    cmx991_load(&cmx991, 0x23, 0x07);

    assert(0x8f == cmx991_pack(&cmx991, 0x11));
    assert(0x50 == cmx991_pack(&cmx991, 0x14));
    assert(0x14 == cmx991_pack(&cmx991, 0x15));
    assert(0x00 == cmx991_pack(&cmx991, 0x16));
    assert(0xc0 == cmx991_pack(&cmx991, 0x20));
    assert(0xa0 == cmx991_pack(&cmx991, 0x21));
    assert(0x08 == cmx991_pack(&cmx991, 0x22));
    assert(0x07 == cmx991_pack(&cmx991, 0x23));
    printf("passed\n");
}

void test_cmx991_pll_enable() {
    printf("test_cmx991_pack_unpack... ");
    cmx991_t cmx991;
    cmx991_init(&cmx991);
    cmx991_pll_enable(&cmx991, 19.2e6, 180.0e6);
    printf("m=%d, n=%d, actual=%f\n", cmx991_get_m(&cmx991), cmx991_get_n(&cmx991), cmx991_pll_actual_frequency(&cmx991, 19.2e6));
    printf("passed\n");
}

int main() {
    test_cmx991_unpack_state();
    test_cmx991_pack_unpack();
    test_cmx991_pll_enable();
    return 0;
}
