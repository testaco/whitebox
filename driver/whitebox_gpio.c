#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "whitebox.h"
#include "whitebox_gpio.h"

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int whitebox_gpio_debug = 0;

/*
 * User can change verbosity of the driver
 */
module_param(whitebox_gpio_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(whitebox_gpio_debug, "whitebox gpio debugging level, >0 is verbose");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (whitebox_gpio_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void whitebox_gpio_free(struct platform_device* pdev) {
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->adc_s1_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->adc_s2_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->adc_dfs_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->dac_en_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->dac_pd_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->dac_cs_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->radio_resetn_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->radio_cdata_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->radio_sclk_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->radio_rdata_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->radio_csn_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->vco_clk_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->vco_data_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->vco_le_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->vco_ce_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->vco_pdb_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->vco_ld_pin);
}
EXPORT_SYMBOL(whitebox_gpio_free);


#define WHITEBOX_GPIO_REQUEST_OUTPUT(pin, v) \
    ret = gpio_request(WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin, #pin); \
    if (ret < 0) { \
        d_printk(0, "Failed to request gpio " #pin " at pin %d\n", \
            WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin); \
        goto failed_gpio_request; \
    } \
    ret = gpio_direction_output(WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin, v); \
    if (ret < 0) { \
        d_printk(0, "Failed to set gpio " #pin " to output\n", \
            WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin); \
        goto failed_gpio_request; \
    } \
    d_printk(2, "Set gpio " #pin " (%d) to output\n", \
        WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin);

#define WHITEBOX_GPIO_REQUEST_INPUT(pin) \
    ret = gpio_request(WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin, #pin); \
    if (ret < 0) { \
        d_printk(0, "Failed to request gpio " #pin " at pin %d\n", \
            WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin); \
        goto failed_gpio_request; \
    } \
    ret = gpio_direction_input(WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin); \
    if (ret < 0) { \
        d_printk(0, "Failed to set gpio " #pin " to input\n", \
            WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin); \
        goto failed_gpio_request; \
    } \
    d_printk(2, "Set gpio " #pin " (%d) to input\n", \
        WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin);

int whitebox_gpio_request(struct platform_device* pdev) {
    int ret = 0;

    WHITEBOX_GPIO_REQUEST_OUTPUT(adc_s1, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(adc_s2, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(adc_dfs, 1); // two's complement
    WHITEBOX_GPIO_REQUEST_OUTPUT(dac_en, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(dac_pd, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(dac_cs, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(radio_resetn, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(radio_cdata, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(radio_sclk, 0);
    WHITEBOX_GPIO_REQUEST_INPUT (radio_rdata);
    WHITEBOX_GPIO_REQUEST_OUTPUT(radio_csn, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(vco_clk, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(vco_data, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(vco_le, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(vco_ce, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(vco_pdb, 1);
    WHITEBOX_GPIO_REQUEST_INPUT(vco_ld);

    goto gpio_request_done;

failed_gpio_request:
gpio_request_done:
    return ret;
}
EXPORT_SYMBOL(whitebox_gpio_request);

void whitebox_gpio_dac_enable(struct whitebox_platform_data_t* platform_data) {
    d_printk(2, "\n");
    gpio_set_value(platform_data->dac_pd_pin, 0);
    //gpio_set_value(platform_data->dac_en_pin, 1);
    gpio_set_value(platform_data->dac_cs_pin, 0);
}
EXPORT_SYMBOL(whitebox_gpio_dac_enable);

void whitebox_gpio_dac_disable(struct whitebox_platform_data_t* platform_data) {
    d_printk(2, "\n");
    gpio_set_value(platform_data->dac_cs_pin, 1);
    //gpio_set_value(platform_data->dac_en_pin, 0);
    gpio_set_value(platform_data->dac_pd_pin, 1);
}
EXPORT_SYMBOL(whitebox_gpio_dac_disable);

void whitebox_gpio_adc_enable(struct whitebox_platform_data_t *platform_data)
{
    gpio_set_value(platform_data->adc_s1_pin, 1);
    gpio_set_value(platform_data->adc_s2_pin, 1);
}
//EXPORT_SYMBOL(whitebox_gpio_adc_enable);

void whitebox_gpio_adc_disable(struct whitebox_platform_data_t *platform_data)
{
    gpio_set_value(platform_data->adc_s1_pin, 0);
    gpio_set_value(platform_data->adc_s2_pin, 0);
}
//EXPORT_SYMBOL(whitebox_gpio_adc_enable);

void whitebox_gpio_cmx991_reset(struct whitebox_platform_data_t* platform_data) {
    d_printk(2, "\n");
    gpio_set_value(platform_data->radio_resetn_pin, 0);
    udelay(10);
    gpio_set_value(platform_data->radio_resetn_pin, 1);
}
EXPORT_SYMBOL(whitebox_gpio_cmx991_reset);

void _cmx991_wr_byte(struct whitebox_platform_data_t* platform_data, u8 byte) {
    int i;

    for (i = 0; i < 8; ++i) {
        gpio_set_value(platform_data->radio_cdata_pin,
                ((byte << i) & 0x80) ? 1 : 0);
        gpio_set_value(platform_data->radio_sclk_pin, 0);
        gpio_set_value(platform_data->radio_sclk_pin, 1);
    }
}

void whitebox_gpio_cmx991_write(struct whitebox_platform_data_t* platform_data, u8 address, u8 data) {
    gpio_set_value(platform_data->radio_csn_pin, 0);
    _cmx991_wr_byte(platform_data, address);
    udelay(10);
    _cmx991_wr_byte(platform_data, data);
    gpio_set_value(platform_data->radio_csn_pin, 1);
    gpio_set_value(platform_data->radio_sclk_pin, 0);
    d_printk(2, "write %x=%x\n", address, data);
}
EXPORT_SYMBOL(whitebox_gpio_cmx991_write);

u8 _cmx991_rd_byte(struct whitebox_platform_data_t* platform_data) {
    u8 i;
    u8 byte = 0;

    for (i = 0; i < 8; ++i) {
        gpio_set_value(platform_data->radio_sclk_pin, 0);
        byte <<= 1;
        gpio_set_value(platform_data->radio_sclk_pin, 1);
        if (gpio_get_value(platform_data->radio_rdata_pin))
            byte |= 0x01;
    }
    return byte;
}

u8 whitebox_gpio_cmx991_read(struct whitebox_platform_data_t* platform_data, u8 address) {
    u8 value;
    gpio_set_value(platform_data->radio_csn_pin, 0);
    _cmx991_wr_byte(platform_data, address);
    udelay(10);
    value = _cmx991_rd_byte(platform_data);
    gpio_set_value(platform_data->radio_csn_pin, 1);
    gpio_set_value(platform_data->radio_sclk_pin, 0);
    d_printk(2, "read %x=%x\n", address, value);
    return value;
}
EXPORT_SYMBOL(whitebox_gpio_cmx991_read);

void whitebox_gpio_adf4351_write(struct whitebox_platform_data_t* platform_data, u32 data) {
    int i;
    d_printk(2, "write %x\n", data);

    // Setup
    gpio_set_value(platform_data->vco_le_pin, 1);
    gpio_set_value(platform_data->vco_clk_pin, 0);

    // Bring LE low to start writing
    gpio_set_value(platform_data->vco_le_pin, 0);

    for (i = 0; i < 32; ++i) {
        // Write Data
        gpio_set_value(platform_data->vco_data_pin,
                ((data << i) & 0x80000000) ? 1: 0);
        // Bring clock high
        gpio_set_value(platform_data->vco_clk_pin, 1);
        // Bring clock low
        gpio_set_value(platform_data->vco_clk_pin, 0);
    }

    // Bring LE high to write register
    gpio_set_value(platform_data->vco_le_pin, 1);

    // Wait for register to latch
    udelay(10);
}
EXPORT_SYMBOL(whitebox_gpio_adf4351_write);

int whitebox_gpio_adf4351_locked(struct whitebox_platform_data_t* platform_data) {
    return gpio_get_value(platform_data->vco_ld_pin);
}
EXPORT_SYMBOL(whitebox_gpio_adf4351_locked);
