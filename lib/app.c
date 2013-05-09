/*
 * This is a user-space application that interacts with the radio
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "adf4351.h"

/*
 * These whitebox pin to Linux kernel GPIO mappings are derived from the
 * Whitebox Libero SmartDesign.
 */
#define ADC_S2       0
#define ADC_S1       1
#define ADC_DFS      2
#define DAC_EN       3
#define DAC_PD       4
#define DAC_CS       5
#define RADIO_RESETN 6
#define RADIO_CDATA  7
#define RADIO_SCLK   8
#define RADIO_RDATA  9
#define RADIO_CSN    10
#define VCO_CLK      11
#define VCO_DATA     12
#define VCO_LE       13
#define VCO_CE       14
#define VCO_PDB      15
#define VCO_LD       16

#define GPIO_OUTPUT_MODE (0 << 0)
#define GPIO_INPUT_MODE  (1 << 0)

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

void radio_wr_byte(uint8_t byte) {
    int i, j;

    for (i = 0; i < 8; ++i) {
        GPIO_set_output(RADIO_CDATA, ((byte << i) & 0x80) ? 1 : 0);
        GPIO_set_output(RADIO_SCLK, 0);
        GPIO_set_output(RADIO_SCLK, 1);
    }
}

void radio_wr1(uint8_t address, uint8_t data) {
    GPIO_set_output(RADIO_CSN, 0);
    radio_wr_byte(address);
    usleep(10);
    radio_wr_byte(data);
    GPIO_set_output(RADIO_CSN, 1);
    GPIO_set_output(RADIO_SCLK, 0);
}

uint8_t radio_rd_byte() {
    uint8_t i;
    uint8_t byte = 0;

    for (i = 0; i < 8; ++i) {
        GPIO_set_output(RADIO_SCLK, 0);
        byte <<= 1;
        GPIO_set_output(RADIO_SCLK, 1);
        if (GPIO_get_input(RADIO_RDATA))
            byte |= 0x01;
    }
    return byte;
}

uint8_t radio_rd1(uint8_t address) {
    uint8_t value;
    GPIO_set_output(RADIO_CSN, 0);
    radio_wr_byte(address);
    usleep(10);
    value = radio_rd_byte();
    GPIO_set_output(RADIO_CSN, 1);
    GPIO_set_output(RADIO_SCLK, 0);
    return value;
}

void radio_init() {
    GPIO_config(RADIO_CSN, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_SCLK, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_CDATA, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_RDATA, GPIO_INPUT_MODE);
}

void radio_power_down() {
    GPIO_set_output(VCO_CE, 0);
}

void radio_power_up() {
    radio_wr1(0x10, 0x00);
}

void vco_init() {
    GPIO_config(VCO_LE, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_CE, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_PDB, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_CLK, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_DATA, GPIO_OUTPUT_MODE);
}

void vco_power_down() {
    GPIO_set_output(VCO_CE, 0);
    GPIO_set_output(VCO_PDB, 0);
}

void vco_power_up() {
    GPIO_set_output(VCO_CE, 1);
    GPIO_set_output(VCO_PDB, 1);
}


void vco_dial(uint32_t data) {
    int i;
    // Setup
    GPIO_set_output(VCO_LE, 1);
    GPIO_set_output(VCO_CLK, 0);

    // Bring LE low to start writing
    GPIO_set_output(VCO_LE, 0);

    for (i = 0; i < 32; ++i) {
        // Write Data
        GPIO_set_output(VCO_DATA, ((data << i) & 0x80000000) ? 1: 0);
        // Bring clock high
        GPIO_set_output(VCO_CLK, 1);
        // Bring clock low
        GPIO_set_output(VCO_CLK, 0);
    }

    // Bring LE high to write register
    GPIO_set_output(VCO_LE, 1);

    for (i = 0; i < 10000; ++i) {}
}

void dac_init() {
    GPIO_config(DAC_CS, GPIO_OUTPUT_MODE);
    GPIO_config(DAC_PD, GPIO_OUTPUT_MODE);
    GPIO_config(DAC_EN, GPIO_OUTPUT_MODE);
}

void dac_power_down() {
    GPIO_set_output(DAC_PD, 1);
    GPIO_set_output(DAC_CS, 1);
}

void dac_power_up() {
    GPIO_set_output(DAC_EN, 0);
    GPIO_set_output(DAC_PD, 0);
    GPIO_set_output(DAC_CS, 1);
}

void dac_tx() {
    GPIO_set_output(DAC_EN, 1);
    GPIO_set_output(DAC_CS, 0);
    sleep(10);
    GPIO_set_output(DAC_CS, 1);
    GPIO_set_output(DAC_EN, 0);
}

int main(int argc, char **argv) {
	char * app_name = argv[0];
	char * dev_name = "/dev/whitebox";
	int ret = -1;
	int c, x;

    if (argc == 1) {
        fprintf(stderr, "You must submit a command!\n");
        ret = 1;
        goto Done;
    }

    vco_init();
    radio_init();
    dac_init();

    if(strcmp(argv[1], "power_down") == 0) {
        fprintf(stdout, "Powering down everything...\n");
        vco_power_down();
        radio_power_down();
        dac_power_down();
    }

    if(strcmp(argv[1], "power_up") == 0) {
        fprintf(stdout, "Powering up everything...\n");
        vco_power_up();
        radio_power_up();
        dac_power_up();
    }

    if(strcmp(argv[1], "dial") == 0) {
        fprintf(stdout, "Dialing VCO...\n");

        adf4351_t adf4351;
        adf4351_init(&adf4351);
        adf4351.charge_pump_current = CHARGE_PUMP_CURRENT_2_50MA;
        adf4351_tune(&adf4351, 198.000e6);
        adf4351_print_to_file(&adf4351, stdout);
        vco_dial(adf4351_pack(&adf4351, 5));
        vco_dial(adf4351_pack(&adf4351, 4));
        vco_dial(adf4351_pack(&adf4351, 3));
        vco_dial(adf4351_pack(&adf4351, 2));
        vco_dial(adf4351_pack(&adf4351, 1));
        vco_dial(adf4351_pack(&adf4351, 0));
        printf("Acutal frequency: %f", adf4351_actual_frequency(&adf4351));
    }

    if(strcmp(argv[1], "tx_tune") == 0) {
        fprintf(stdout, "Reading to transmit...\n");
        radio_wr1(0x11, 0x8f);
        fprintf(stdout, "read %02x\n", radio_rd1(0xe1));
        radio_wr1(0x14, 0x50);
        fprintf(stdout, "read %02x\n", radio_rd1(0xe4));
        radio_wr1(0x15, 0x14);
        fprintf(stdout, "read %02x\n", radio_rd1(0xe5));
        radio_wr1(0x16, 0x00);
        fprintf(stdout, "read %02x\n", radio_rd1(0xe6));
        radio_wr1(0x20, 0xc0);
        fprintf(stdout, "read %02x\n", radio_rd1(0xd0));
        radio_wr1(0x21, 0xa0);
        fprintf(stdout, "read %02x\n", radio_rd1(0xd1));
        radio_wr1(0x22, 0x08);
        fprintf(stdout, "read %02x\n", radio_rd1(0xd2));
        radio_wr1(0x23, 0x07);
        fprintf(stdout, "read %02x\n", radio_rd1(0xd3));
    }

    if(strcmp(argv[1], "tx") == 0) {
        int locked_register = radio_rd1(0xd1);
        if (!(locked_register & 0x40)) {
            fprintf(stdout, "IF not locked!, %02x\n", locked_register);
        } else {
            fprintf(stdout, "Turning on dac...\n");
            dac_tx();
        }
    }

	/*
 	 * If we are here, we have been successful
 	 */
	ret = 0;

Done:
	return ret;
}
