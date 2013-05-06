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

#include "TOPLEVEL_hw_platform.h"
#include "drivers/CoreGPIO/core_gpio.h"
#include "drivers/mss_pdma/mss_pdma.h"

#include "ic_regs/adf4351_regs.h"

#define VCO_PDB GPIO_3
#define VCO_PDB_MASK GPIO_3_MASK
#define VCO_CE GPIO_4
#define VCO_CE_MASK GPIO_4_MASK
#define VCO_CLK GPIO_5
#define VCO_CLK_MASK GPIO_5_MASK
#define VCO_LE GPIO_6
#define VCO_LE_MASK GPIO_6_MASK
#define VCO_DATA GPIO_7
#define VCO_DATA_MASK GPIO_7_MASK

#define RADIO_RDATA GPIO_9
#define RADIO_RDATA_MASK GPIO_9_MASK
#define RADIO_CSN GPIO_10
#define RADIO_CSN_MASK GPIO_10_MASK
#define RADIO_SCLK GPIO_11
#define RADIO_SCLK_MASK GPIO_11_MASK
#define RADIO_CDATA GPIO_12
#define RADIO_CDATA_MASK GPIO_12_MASK

#define DAC_CS GPIO_13
#define DAC_CS_MASK GPIO_13_MASK
#define DAC_PD GPIO_14
#define DAC_PD_MASK GPIO_14_MASK
#define DAC_EN GPIO_15
#define DAC_EN_MASK GPIO_15_MASK

gpio_instance_t g_gpio;

void radio_wr_byte(uint8_t byte) {
    int i, j;

    for (i = 0; i < 8; ++i) {
        GPIO_set_output(&g_gpio, RADIO_CDATA, ((byte << i) & 0x80) ? 1 : 0);
        GPIO_set_output(&g_gpio, RADIO_SCLK, 0);
        GPIO_set_output(&g_gpio, RADIO_SCLK, 1);
    }
}

void radio_wr1(uint8_t address, uint8_t data) {
    GPIO_set_output(&g_gpio, RADIO_CSN, 0);
    radio_wr_byte(address);
    usleep(10);
    radio_wr_byte(data);
    GPIO_set_output(&g_gpio, RADIO_CSN, 1);
    GPIO_set_output(&g_gpio, RADIO_SCLK, 0);
}

uint8_t radio_rd_byte() {
    uint8_t i;
    uint8_t byte = 0;

    for (i = 0; i < 8; ++i) {
        GPIO_set_output(&g_gpio, RADIO_SCLK, 0);
        byte <<= 1;
        GPIO_set_output(&g_gpio, RADIO_SCLK, 1);
        if (GPIO_get_inputs(&g_gpio) & RADIO_RDATA_MASK)
            byte |= 0x01;
    }
    return byte;
}

uint8_t radio_rd1(uint8_t address) {
    uint8_t value;
    GPIO_set_output(&g_gpio, RADIO_CSN, 0);
    radio_wr_byte(address);
    usleep(10);
    value = radio_rd_byte();
    GPIO_set_output(&g_gpio, RADIO_CSN, 1);
    GPIO_set_output(&g_gpio, RADIO_SCLK, 0);
    return value;
}

void radio_init() {
    GPIO_config(&g_gpio, RADIO_CSN, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, RADIO_SCLK, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, RADIO_CDATA, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, RADIO_RDATA, GPIO_INPUT_MODE);
}

void radio_power_down() {
    GPIO_set_output(&g_gpio, VCO_CE, 0);
}

void radio_power_up() {
    radio_wr1(0x10, 0x00);
}

void vco_init() {
    GPIO_config(&g_gpio, VCO_LE, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, VCO_CE, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, VCO_PDB, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, VCO_CLK, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, VCO_DATA, GPIO_OUTPUT_MODE);
}

void vco_power_down() {
    GPIO_set_output(&g_gpio, VCO_CE, 0);
    GPIO_set_output(&g_gpio, VCO_PDB, 0);
}

void vco_power_up() {
    GPIO_set_output(&g_gpio, VCO_CE, 1);
    GPIO_set_output(&g_gpio, VCO_PDB, 1);
}


void vco_dial(uint32_t data) {
    int i;
    // Setup
    GPIO_set_output(&g_gpio, VCO_LE, 1);
    GPIO_set_output(&g_gpio, VCO_CLK, 0);

    // Bring LE low to start writing
    GPIO_set_output(&g_gpio, VCO_LE, 0);

    for (i = 0; i < 32; ++i) {
        // Write Data
        GPIO_set_output(&g_gpio, VCO_DATA, ((data << i) & 0x80000000) ? 1: 0);
        // Bring clock high
        GPIO_set_output(&g_gpio, VCO_CLK, 1);
        // Bring clock low
        GPIO_set_output(&g_gpio, VCO_CLK, 0);
    }

    // Bring LE high to write register
    GPIO_set_output(&g_gpio, VCO_LE, 1);

    for (i = 0; i < 10000; ++i) {}
}

void dac_init() {
    GPIO_config(&g_gpio, DAC_CS, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, DAC_PD, GPIO_OUTPUT_MODE);
    GPIO_config(&g_gpio, DAC_EN, GPIO_OUTPUT_MODE);
}

void dac_power_down() {
    GPIO_set_output(&g_gpio, DAC_PD, 1);
    GPIO_set_output(&g_gpio, DAC_CS, 1);
}

void dac_power_up() {
    GPIO_set_output(&g_gpio, DAC_EN, 0);
    GPIO_set_output(&g_gpio, DAC_PD, 0);
    GPIO_set_output(&g_gpio, DAC_CS, 1);
}

void dac_tx() {
    GPIO_set_output(&g_gpio, DAC_EN, 1);
    GPIO_set_output(&g_gpio, DAC_CS, 0);
    sleep(10);
    GPIO_set_output(&g_gpio, DAC_CS, 1);
    GPIO_set_output(&g_gpio, DAC_EN, 0);
}

int main(int argc, char **argv) {
	char * app_name = argv[0];
	char * dev_name = "/dev/sample";
	int ret = -1;
	int fd = -1;
	int c, x;

    GPIO_init(&g_gpio, COREGPIO_0, GPIO_APB_32_BITS_BUS);
    PDMA_init();

    if (argc == 1) {
        fprintf(stderr, "You must submit a command!\n");
        ret = 1;
        goto Done;
    }

    vco_init();
    radio_init();
    dac_init();
    //(*((uint32_t volatile*)(RADIO_0+1)) = 0xffffffff);

    //fprintf(stdout, "Reading %02x\n", *((uint32_t volatile*)(RADIO_0+1)));

    if(strcmp(argv[1], "power_down") == 0) {
        fprintf(stdout, "Powering down everything...\n");
        (*((uint32_t volatile*)(RADIO_0+0)) = 0x0);
        //(*((uint32_t volatile*)(RADIO_0+1)) = 0x0);
        vco_power_down();
        radio_power_down();
        dac_power_down();
    }

    if(strcmp(argv[1], "power_up") == 0) {
        fprintf(stdout, "Powering up everything...\n");
        (*((uint32_t volatile*)(RADIO_0+0)) = 0xffff0000);
        //(*((uint32_t volatile*)(RADIO_0+1)) = 0x3ff);
        vco_power_up();
        radio_power_up();
        dac_power_up();
    }

    if(strcmp(argv[1], "dial") == 0) {
        fprintf(stdout, "Dialing VCO...\n");
        /* 35MHz FULL POWER */
        /*vco_dial(0x00180005);
        vco_dial(0x00EC81FC);
        vco_dial(0x000004B3);
        vco_dial(0x00004EC2);
        vco_dial(0x08000029);
        vco_dial(0x002C8018);*/
        /* 144.39 - APRS 2m */
        /*vco_dial(0x00180005);
        vco_dial(0x00CC81B4);
        vco_dial(0x000004B3);
        vco_dial(0x00004EC2);
        vco_dial(0x08000411);
        vco_dial(0x002C0378);*/
        /* 288.78 - 2x APRS 2m
        vco_dial(0x00180005);
        vco_dial(0x00BC81B4);
        vco_dial(0x000004B3);
        vco_dial(0x00004EC2);
        vco_dial(0x08000411);
        vco_dial(0x002C0378); */


        /* FOR 2m - 198 */
        //vco_dial(0x00180005); //WBX SPI Reg (0x05): 0x00400005
        //vco_dial(0x00CD01FC); //WBX SPI Reg (0x04): 0x0030123c
        //vco_dial(0x000004B3); //WBX SPI Reg (0x03): 0x0001000b
        //vco_dial(0x00004EC2); //WBX SPI Reg (0x02): 0x64b9ca42
        //vco_dial(0x00000069); //WBX SPI Reg (0x01): 0x00001641
        //vco_dial(0x003C8058); //WBX SPI Reg (0x00): 0x161a3b78

        adf4351_regs_t adf4351_regs;
        adf4351_regs_t_init(&adf4351_regs);
        adf4351_regs.charge_pump_current = CHARGE_PUMP_CURRENT_2_50MA;
        adf4351_tune(&adf4351_regs, 198.000e6);
        adf4351_regs_t_print(&adf4351_regs, stdout);
        vco_dial(adf4351_regs_t_pack(&adf4351_regs, 5));
        vco_dial(adf4351_regs_t_pack(&adf4351_regs, 4));
        vco_dial(adf4351_regs_t_pack(&adf4351_regs, 3));
        vco_dial(adf4351_regs_t_pack(&adf4351_regs, 2));
        vco_dial(adf4351_regs_t_pack(&adf4351_regs, 1));
        vco_dial(adf4351_regs_t_pack(&adf4351_regs, 0));
        adf4351_actual_frequency(&adf4351_regs);
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

    if (strcmp(argv[1], "dma") == 0) {
        int i;
        int channel_id = PDMA_CHANNEL_0;
        fprintf(stdout, "Measuring DMA performance...\n");
        for (i = 0; i < 8; ++i) {
            fprintf(stdout, "Channel #%d: %02x\n", i, PDMA_status(i));
        }
        //fprintf(stdout, "Channel #0: %02x\n", PDMA_status(PDMA_CHANNEL_0));
        //PDMA_configure(PDMA_CHANNEL_0, PDMA_TO_SPI_1, PDMA_LOW_PRIORITY | PDMA_BYTE_TRANSFER | PDMA_INC_SRC_ONE_BYTE, PDMA_DEFAULT_WRITE_ADJ);

        // Get start time
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        fprintf(stdout, "Start time %d:%d\n", start.tv_sec, start.tv_nsec);


        int beef[1];
        beef[0] = 0xfff02f;
        PDMA_configure(PDMA_CHANNEL_0,
            PDMA_MEM_TO_MEM,
            PDMA_LOW_PRIORITY | PDMA_WORD_TRANSFER | PDMA_INC_SRC_FOUR_BYTES, 
            PDMA_DEFAULT_WRITE_ADJ);
        PDMA_start(PDMA_CHANNEL_0, (uint32_t)beef, RADIO_0, 1);

        uint32_t status;
        do {
            status = PDMA_status(PDMA_CHANNEL_0);
        } while(status == 0);

        clock_gettime(CLOCK_MONOTONIC, &end);
        int tdiff_ns = ((end.tv_sec - start.tv_sec) * 1000) + (end.tv_nsec - start.tv_nsec);
        fprintf(stdout, "End time %d:%d\n", end.tv_sec, end.tv_nsec);
        fprintf(stdout, "Total time in ns %d\n", tdiff_ns);

    }

	/*
 	 * If we are here, we have been successful
 	 */
	ret = 0;

Done:
	if (fd >= 0) {
		close(fd);
	}
	return ret;
}
