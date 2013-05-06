#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "adf4351.h"

void test_adf4351_pack_unpack() {
    printf("test_adf4351_pack_unpack... ");
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
    printf("passed\n");
}

void test_adf4351_compute_frequency() {
    printf("test_adf4351_compute_frequency... ");
    adf4351_t adf4351;
    adf4351_load(&adf4351, 0x00180005);
    adf4351_load(&adf4351, 0x00CD01FC);
    adf4351_load(&adf4351, 0x000004B3);
    adf4351_load(&adf4351, 0x00004EC2);
    adf4351_load(&adf4351, 0x00000069);
    adf4351_load(&adf4351, 0x003C8058);

    assert(abs(198e6 - adf4351_actual_frequency(&adf4351)) < 1e3);
    printf("passed\n");
}

void test_adf4351_tune() {
    printf("test_adf4351_compute_frequency... ");
    adf4351_t adf4351;
    adf4351_tune(&adf4351, 198e6);
    assert(fabs(198e6 - adf4351_actual_frequency(&adf4351)) < 1e3);

    printf("passed\n");
}

int main() {
    test_adf4351_pack_unpack();
    test_adf4351_compute_frequency();
    test_adf4351_tune();
    printf("All tests passed.\n");
    return 0;
}
