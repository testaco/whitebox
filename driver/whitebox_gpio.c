#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "whitebox.h"
#include "whitebox_gpio.h"

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (whitebox_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

void whitebox_gpio_free(struct platform_device* pdev) {
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->en_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->pd_pin);

    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->radio_cs_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->rflo_cs_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->iflo_cs_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->gw_cs_pin);

    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->mosi_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->sclk_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->miso_pin);

    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->rfld_pin);

    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->adc_s1_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->adc_s2_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->adc_dfs_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->dac_pd_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->dac_cs_pin);

    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->detect_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->amp_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->tr_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->noise_pin);

    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->ptt_out_pin);
    gpio_free(WHITEBOX_PLATFORM_DATA(pdev)->ptt_in_pin);
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
    d_printk(2, "Set gpio " #pin " (%d) to output (default=%d)\n", \
        WHITEBOX_PLATFORM_DATA(pdev)->pin ## _pin, v);

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

    WHITEBOX_GPIO_REQUEST_OUTPUT(en, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(pd, 0);

    WHITEBOX_GPIO_REQUEST_OUTPUT(radio_cs, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(rflo_cs, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(iflo_cs, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(gw_cs, 1);

    WHITEBOX_GPIO_REQUEST_OUTPUT(mosi, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(sclk, 0);
    WHITEBOX_GPIO_REQUEST_INPUT(miso);

    WHITEBOX_GPIO_REQUEST_INPUT(rfld);

    WHITEBOX_GPIO_REQUEST_OUTPUT(adc_s1, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(adc_s2, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(adc_dfs, 1); // two's complement
    WHITEBOX_GPIO_REQUEST_OUTPUT(dac_pd, 1);
    WHITEBOX_GPIO_REQUEST_OUTPUT(dac_cs, 1);

    WHITEBOX_GPIO_REQUEST_OUTPUT(detect, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(amp, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(tr, 0);
    WHITEBOX_GPIO_REQUEST_OUTPUT(noise, 0);

    WHITEBOX_GPIO_REQUEST_OUTPUT(ptt_out, 1);
    WHITEBOX_GPIO_REQUEST_INPUT(ptt_in);

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

void _cmx991_wr_byte(struct whitebox_platform_data_t* platform_data, u8 byte) {
    int i;

    for (i = 0; i < 8; ++i) {
        gpio_set_value(platform_data->mosi_pin,
                ((byte << i) & 0x80) ? 1 : 0);
        gpio_set_value(platform_data->sclk_pin, 0);
        gpio_set_value(platform_data->sclk_pin, 1);
    }
}

void whitebox_gpio_cmx991_reset(struct whitebox_platform_data_t* platform_data) {
    // TODO: This should issue a software reset.
    d_printk(4, "reset\n");
    gpio_set_value(platform_data->radio_cs_pin, 0);
    _cmx991_wr_byte(platform_data, 0x10);
    gpio_set_value(platform_data->radio_cs_pin, 1);
    gpio_set_value(platform_data->sclk_pin, 0);
    udelay(10);
}
EXPORT_SYMBOL(whitebox_gpio_cmx991_reset);

void whitebox_gpio_cmx991_write(struct whitebox_platform_data_t* platform_data, u8 address, u8 data) {
    d_printk(4, "write %x=%x\n", address, data);
    gpio_set_value(platform_data->radio_cs_pin, 0);
    _cmx991_wr_byte(platform_data, address);
    udelay(10);
    _cmx991_wr_byte(platform_data, data);
    gpio_set_value(platform_data->radio_cs_pin, 1);
    gpio_set_value(platform_data->sclk_pin, 0);
}
EXPORT_SYMBOL(whitebox_gpio_cmx991_write);

u8 _cmx991_rd_byte(struct whitebox_platform_data_t* platform_data) {
    u8 i;
    u8 byte = 0;

    for (i = 0; i < 8; ++i) {
        gpio_set_value(platform_data->sclk_pin, 0);
        byte <<= 1;
        gpio_set_value(platform_data->sclk_pin, 1);
        if (gpio_get_value(platform_data->miso_pin))
            byte |= 0x01;
    }
    return byte;
}

u8 whitebox_gpio_cmx991_read(struct whitebox_platform_data_t* platform_data, u8 address) {
    u8 value;
    d_printk(4, "read %x=xx\n", address);
    gpio_set_value(platform_data->radio_cs_pin, 0);
    _cmx991_wr_byte(platform_data, address);
    udelay(10);
    value = _cmx991_rd_byte(platform_data);
    gpio_set_value(platform_data->radio_cs_pin, 1);
    gpio_set_value(platform_data->sclk_pin, 0);
    d_printk(4, "read %x=%x\n", address, value);
    return value;
}
EXPORT_SYMBOL(whitebox_gpio_cmx991_read);

void whitebox_gpio_adf4351_write(struct whitebox_platform_data_t* platform_data, u32 data) {
    int i;
    d_printk(4, "write %x\n", data);

    // Setup
    gpio_set_value(platform_data->rflo_cs_pin, 1);
    gpio_set_value(platform_data->sclk_pin, 0);

    // Bring LE low to start writing
    gpio_set_value(platform_data->rflo_cs_pin, 0);

    for (i = 0; i < 32; ++i) {
        // Write Data
        gpio_set_value(platform_data->mosi_pin,
                ((data << i) & 0x80000000) ? 1: 0);
        // Bring clock high
        gpio_set_value(platform_data->sclk_pin, 1);
        // Bring clock low
        gpio_set_value(platform_data->sclk_pin, 0);
    }

    // Bring LE high to write register
    gpio_set_value(platform_data->rflo_cs_pin, 1);

    // Wait for register to latch
    udelay(10);
}
EXPORT_SYMBOL(whitebox_gpio_adf4351_write);

int whitebox_gpio_adf4351_locked(struct whitebox_platform_data_t* platform_data) {
    return gpio_get_value(platform_data->rfld_pin);
}
EXPORT_SYMBOL(whitebox_gpio_adf4351_locked);

void whitebox_gpio_adf4360_write(struct whitebox_platform_data_t* platform_data, u32 data) {
    int i;
    d_printk(4, "write %x\n", data);

    // Setup
    gpio_set_value(platform_data->iflo_cs_pin, 1);
    gpio_set_value(platform_data->sclk_pin, 0);

    // Bring LE low to start writing
    gpio_set_value(platform_data->iflo_cs_pin, 0);

    for (i = 0; i < 24; ++i) {
        // Write Data
        gpio_set_value(platform_data->mosi_pin,
                ((data << i) & 0x00800000) ? 1: 0);
        // Bring clock high
        gpio_set_value(platform_data->sclk_pin, 1);
        // Bring clock low
        gpio_set_value(platform_data->sclk_pin, 0);
    }

    // Bring LE high to write register
    gpio_set_value(platform_data->iflo_cs_pin, 1);

    // Wait for register to latch
    udelay(10);
}
EXPORT_SYMBOL(whitebox_gpio_adf4360_write);

int whitebox_gpio_adf4360_locked(struct whitebox_platform_data_t* platform_data) {
    // TODO: use the mux detect for this?.
    //return gpio_get_value(platform_data->vco_ld_pin);
    return 0;
}
EXPORT_SYMBOL(whitebox_gpio_adf4360_locked);

int whitebox_gpio_analog_enable(struct whitebox_platform_data_t* platform_data) {
    d_printk(1, "turning on the analog portion of the board\n");
    gpio_set_value(platform_data->en_pin, 1);
    // TODO: for now...
    gpio_set_value(platform_data->detect_pin, 0);
    gpio_set_value(platform_data->amp_pin, 0);
    gpio_set_value(platform_data->tr_pin, 0);
    gpio_set_value(platform_data->noise_pin, 1);
    return 0;
}
EXPORT_SYMBOL(whitebox_gpio_analog_enable);

int whitebox_gpio_analog_disable(struct whitebox_platform_data_t* platform_data) {
    d_printk(1, "turning off the analog portion of the board\n");
    gpio_set_value(platform_data->en_pin, 0);
    // TODO: for now...
    gpio_set_value(platform_data->detect_pin, 0);
    gpio_set_value(platform_data->amp_pin, 0);
    gpio_set_value(platform_data->tr_pin, 0);
    gpio_set_value(platform_data->noise_pin, 0);
    return 0;
}
EXPORT_SYMBOL(whitebox_gpio_analog_disable);

void whitebox_gpio_gateway_write(
    struct whitebox_platform_data_t *platform_data, u8 data) {
    int i;
    d_printk(1, "write %x\n", data);

    // Setup
    gpio_set_value(platform_data->gw_cs_pin, 1);
    gpio_set_value(platform_data->sclk_pin, 0);

    // Bring CE low to start writing
    gpio_set_value(platform_data->gw_cs_pin, 0);

    for (i = 0; i < 8; ++i) {
        // Write Data
        gpio_set_value(platform_data->mosi_pin,
                ((data << i) & 0x00000080) ? 1: 0);
        // Bring clock high
        gpio_set_value(platform_data->sclk_pin, 1);
        // Bring clock low
        gpio_set_value(platform_data->sclk_pin, 0);
    }

    // Bring CE high to write register
    gpio_set_value(platform_data->gw_cs_pin, 1);

    // Wait for register to latch
    udelay(10);
}

int whitebox_gpio_gateway_control(
    struct whitebox_platform_data_t *platform_data,
    u8 transmit, u8 amplifier, u8 detect, u8 noise, u8 bpf)
{
    d_printk(1, "write tx=%d amp=%d det=%d noise=%d bpf=%d\n",
        transmit, amplifier, detect, noise, bpf);
    gpio_set_value(platform_data->tr_pin, transmit);
    gpio_set_value(platform_data->amp_pin, amplifier);
    gpio_set_value(platform_data->detect_pin, detect);
    gpio_set_value(platform_data->noise_pin, noise);

    whitebox_gpio_gateway_write(platform_data, 1 << bpf);
    return 0;
}
EXPORT_SYMBOL(whitebox_gpio_gateway_control);
