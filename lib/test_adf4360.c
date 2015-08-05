#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>

#include "adf4360.h"

#include <whitebox_ioctl.h>
#include "whitebox_test.h"

#define WHITEBOX_DEV "/dev/whitebox"
int test_adf4360_pack_unpack(void *data) {
    adf4360_t adf4360;
    adf4360_load(&adf4360, 0x003000C9);
    adf4360_load(&adf4360, 0x004FF210);
    adf4360_load(&adf4360, 0x00007022);

    assert(0x004FF210 == adf4360_pack(&adf4360, 0));
    assert(0x003000C9 == adf4360_pack(&adf4360, 1));
    assert(0x00007022 == adf4360_pack(&adf4360, 2));

    return 0;
}

int test_adf4360_compute_frequency(void *data) {
    adf4360_t adf4360;
    adf4360_load(&adf4360, 0x003000C9);
    adf4360_load(&adf4360, 0x004FF210);
    adf4360_load(&adf4360, 0x00007022);
    assert(abs(360e6 - adf4360_actual_frequency(&adf4360, 10e6)) < 1e3);
    return 0;
}

int test_adf4360_tune(void *data) {
    adf4360_t adf4360;
    adf4360_pll_enable(&adf4360, 10e6, 200e3, 360e6);
    assert(fabs(360e6 - adf4360_actual_frequency(&adf4360, 10e6)) < 1e3);
    return 0;
}

int test_adf4360_responds(void *data) {
    int fd;
    whitebox_args_t w;
    adf4360_t adf4360;
    printf("Opening\n");
    fd = open(WHITEBOX_DEV, O_WRONLY);
    assert(fd > 0);
    adf4360_ioctl_get(&adf4360, &w);
    adf4360_pll_enable(&adf4360, 10e6, 200e3, 360e6);
    adf4360_ioctl_set(&adf4360, &w);
    ioctl(fd, WA60_SET, &w);
    //sleep(3);

    printf("VDD\n");
    adf4360.adf4360_muxout = ADF4360_MUXOUT_DVDD;
    adf4360_ioctl_set(&adf4360, &w);
    ioctl(fd, WA60_SET, &w);
    //assert(ioctl(fd, WA60_LOCKED));
    //sleep(3);

    printf("DGND\n");
    adf4360.adf4360_muxout = ADF4360_MUXOUT_DGND;
    adf4360_ioctl_set(&adf4360, &w);
    ioctl(fd, WA60_SET, &w);
    //assert(!ioctl(fd, WA60_LOCKED));
    //sleep(3);

    printf("RDIV\n");
    adf4360.adf4360_muxout = ADF4360_MUXOUT_RDIV;
    adf4360_ioctl_set(&adf4360, &w);
    ioctl(fd, WA60_SET, &w);
    sleep(3);

    printf("NDIV\n");
    adf4360.adf4360_muxout = ADF4360_MUXOUT_NDIV;
    adf4360_ioctl_set(&adf4360, &w);
    ioctl(fd, WA60_SET, &w);
    sleep(5);

    printf("DLD\n");
    adf4360.adf4360_muxout = ADF4360_MUXOUT_DLD;
    adf4360_ioctl_set(&adf4360, &w);
    ioctl(fd, WA60_SET, &w);
    sleep(100);

    close(fd);
}

int main(int argc, char **argv) {
    whitebox_test_t tests[] = {
        WHITEBOX_TEST(test_adf4360_pack_unpack),
        WHITEBOX_TEST(test_adf4360_compute_frequency),
        WHITEBOX_TEST(test_adf4360_tune),
        WHITEBOX_TEST(test_adf4360_responds),
        WHITEBOX_TEST(0),
    };
    return whitebox_test_main(tests, NULL, argc, argv);
}
