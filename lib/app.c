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
#include "cmx991.h"

/*
 * These whitebox pin to Linux kernel GPIO mappings are derived from the
 * Whitebox Libero SmartDesign.  */
#define FPGA_GPIO_BASE 32
#define ADC_S2       (FPGA_GPIO_BASE+3)
#define ADC_S1       (FPGA_GPIO_BASE+4)
#define ADC_DFS      (FPGA_GPIO_BASE+5)
#define DAC_EN       (FPGA_GPIO_BASE+6)
#define DAC_PD       (FPGA_GPIO_BASE+7)
#define DAC_CS       (FPGA_GPIO_BASE+8)
#define RADIO_RESETN (FPGA_GPIO_BASE+9)
#define RADIO_CDATA  (FPGA_GPIO_BASE+10)
#define RADIO_SCLK   (FPGA_GPIO_BASE+11)
#define RADIO_RDATA  (FPGA_GPIO_BASE+12)
#define RADIO_CSN    (FPGA_GPIO_BASE+13)
#define VCO_CLK      (FPGA_GPIO_BASE+14)
#define VCO_DATA     (FPGA_GPIO_BASE+15)
#define VCO_LE       (FPGA_GPIO_BASE+16)
#define VCO_CE       (FPGA_GPIO_BASE+17)
#define VCO_PDB      (FPGA_GPIO_BASE+18)
#define VCO_LD       (FPGA_GPIO_BASE+19)

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
    puts("radio_init");
    GPIO_config(RADIO_CSN, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_SCLK, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_CDATA, GPIO_OUTPUT_MODE);
    GPIO_config(RADIO_RDATA, GPIO_INPUT_MODE);
    GPIO_config(RADIO_RESETN, GPIO_OUTPUT_MODE);
    radio_wr1(0x10, 0x00);
}

void radio_power_down() {
    puts("radio_power_down");
    GPIO_set_output(RADIO_RESETN, 0);
}

void radio_power_up() {
    puts("radio_power_up");
    GPIO_set_output(RADIO_RESETN, 1);
    radio_wr1(0x10, 0x00);
}

void vco_init() {
    puts("vco_init");
    GPIO_config(VCO_LE, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_CE, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_PDB, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_CLK, GPIO_OUTPUT_MODE);
    GPIO_config(VCO_DATA, GPIO_OUTPUT_MODE);
}

void vco_power_down() {
    puts("vco_power_down");
    GPIO_set_output(VCO_CE, 0);
    GPIO_set_output(VCO_PDB, 0);
}

void vco_power_up() {
    puts("vco_power_up");
    GPIO_set_output(VCO_CE, 1);
    GPIO_set_output(VCO_PDB, 1);
}


void vco_dial(uint32_t data) {
    int i;
    puts("vco_dial");
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
    puts("dac_init");
    GPIO_config(DAC_CS, GPIO_OUTPUT_MODE);
    GPIO_config(DAC_PD, GPIO_OUTPUT_MODE);
    GPIO_config(DAC_EN, GPIO_OUTPUT_MODE);
}

void dac_power_down() {
    puts("dac_power_down");
    GPIO_set_output(DAC_PD, 1);
    GPIO_set_output(DAC_CS, 1);
}

void dac_power_up() {
    puts("dac_power_up");
    GPIO_set_output(DAC_EN, 0);
    GPIO_set_output(DAC_PD, 0);
    GPIO_set_output(DAC_CS, 1);
}

void dac_tx() {
    puts("dac_tx");
    GPIO_set_output(DAC_EN, 1);
    GPIO_set_output(DAC_CS, 0);
    sleep(10);
    GPIO_set_output(DAC_CS, 1);
    GPIO_set_output(DAC_EN, 0);
}

void adc_init() {
    puts("adc_init");
    GPIO_config(ADC_S1, GPIO_OUTPUT_MODE);
    GPIO_config(ADC_S2, GPIO_OUTPUT_MODE);
    GPIO_config(ADC_DFS, GPIO_OUTPUT_MODE);
}

void adc_power_down() {
    puts("adc_power_down");
    GPIO_set_output(ADC_S1, 0);
    GPIO_set_output(ADC_S2, 0);
}

void adc_power_up() {
    puts("adc_power_up");
    GPIO_set_output(ADC_S1, 1);
    GPIO_set_output(ADC_S2, 0);
    GPIO_set_output(ADC_DFS, 1); // TWO's COMPLEMENT
}

int main(int argc, char **argv) {
	char * app_name = argv[0];
	char * dev_name = "/dev/whitebox";
	int ret = -1;
	int c, x;

    cmx991_t cmx991;
    cmx991_init(&cmx991);

    if (argc == 1) {
        fprintf(stderr, "You must submit a command!\n");
        ret = 1;
        goto Done;
    }

    if(strcmp(argv[1], "init") == 0) {
        vco_init();
        radio_init();
        dac_init();
        adc_init();
    }

    if(strcmp(argv[1], "power_down") == 0) {
        fprintf(stdout, "Powering down everything...\n");
        vco_power_down();
        radio_power_down();
        dac_power_down();
        adc_power_down();
    }

    if(strcmp(argv[1], "power_up") == 0) {
        fprintf(stdout, "Powering up everything...\n");
        vco_power_up();
        radio_power_up();
        dac_power_up();
        //adc_power_up();
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
        cmx991_resume(&cmx991);
        if (cmx991_pll_enable(&cmx991, 19.2e6, 45.00e6) < 0) {
            fprintf(stderr, "Error setting the pll\n");
        }
        cmx991_tx_tune(&cmx991, 198.00e6, IF_FILTER_BW_120MHZ, HI_LO_HIGHER,
            TX_RF_DIV_BY_2, TX_IF_DIV_BY_2, GAIN_P0DB);
        cmx991_print_to_file(&cmx991, stdout);

        radio_wr1(0x11, cmx991_pack(&cmx991, 0x11));
        fprintf(stdout, "read 0x11 %02x\n", radio_rd1(0xe1));
        radio_wr1(0x14, cmx991_pack(&cmx991, 0x14));
        fprintf(stdout, "read 0x14 %02x\n", radio_rd1(0xe4));
        radio_wr1(0x15, cmx991_pack(&cmx991, 0x15));
        fprintf(stdout, "read 0x15 %02x\n", radio_rd1(0xe5));
        radio_wr1(0x16, cmx991_pack(&cmx991, 0x16));
        fprintf(stdout, "read 0x16 %02x\n", radio_rd1(0xe6));
        radio_wr1(0x20, cmx991_pack(&cmx991, 0x20));
        fprintf(stdout, "read 0x20 %02x\n", radio_rd1(0xd0));
        radio_wr1(0x21, cmx991_pack(&cmx991, 0x21));
        fprintf(stdout, "read 0x21 %02x\n", radio_rd1(0xd1));
        radio_wr1(0x22, cmx991_pack(&cmx991, 0x22));
        fprintf(stdout, "read 0x22 %02x\n", radio_rd1(0xd2));
        radio_wr1(0x23, cmx991_pack(&cmx991, 0x23));
        fprintf(stdout, "read 0x23 %02x\n", radio_rd1(0xd3));
    }

    if(strcmp(argv[1], "tx") == 0) {
        cmx991_load(&cmx991, 0x21, radio_rd1(0xd1));
        if (!cmx991_pll_locked(&cmx991)) {
            fprintf(stdout, "IF not locked!\n");
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
