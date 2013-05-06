#include <assert.h>

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
    assert(abs(180.0e6 == cmx991_actual_frequency(&cmx991, 19.2e6)) < 1);

    printf("passed\n");
}

int main() {
    test_cmx991_unpack_state();
    return 0;
}
