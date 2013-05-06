#include <assert.h>
#include <stdio.h>

#include "radio.h"

void test_radio_create() {
    printf("test_radio_create... ");
    radio_t t;
    radio_init(&t);
    printf("passed\n");
}

int main() {
    test_radio_create();
    printf("All tests passed.\n");
    return 0;
}
