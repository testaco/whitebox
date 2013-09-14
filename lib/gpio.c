#include <sys/stat.h> 
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "gpio.h"

void GPIO_config(unsigned gpio, int inout) {
    int fd;
    char buf[512];

    fd = open("/sys/class/gpio/export", O_WRONLY);
    sprintf(buf, "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);

    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if (inout == GPIO_OUTPUT_MODE)
        write(fd, "out", 3);
    else
        write(fd, "in", 2);
    close(fd);
}

void GPIO_set_output(unsigned gpio, unsigned value) {
    int fd;
    char buf[512];
    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(buf, O_WRONLY);
    if (value)
        write(fd, "1", 1);
    else
        write(fd, "0", 1);
    close(fd);
}

int GPIO_get_input(unsigned gpio) {
    char value;
    int ret;
    int fd;
    char buf[512];
    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(buf, O_RDONLY);
    read(fd, &value, 1);
    ret = value == '0' ? 0 : 1;
    close(fd);
    return ret;
}

