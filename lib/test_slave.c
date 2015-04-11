#include <stdint.h>

#define SLAVE_ADDR 0x31000000

int main(int argc, char **argv) {
    uint32_t volatile *reg = (uint32_t*)SLAVE_ADDR;
    *reg = 1;
}
