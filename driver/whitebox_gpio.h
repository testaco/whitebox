#ifndef __WHITEBOX_GPIO_H
#define __WHITEBOX_GPIO_H

struct platform_device;
struct whitebox_platform_data_t;

void whitebox_gpio_free(struct platform_device* pdev);
int whitebox_gpio_request(struct platform_device* pdev);

void whitebox_gpio_dac_enable(
        struct whitebox_platform_data_t* platform_data);
void whitebox_gpio_dac_disable(
        struct whitebox_platform_data_t* platform_data);

void whitebox_gpio_adc_enable(
        struct whitebox_platform_data_t* platform_data);
void whitebox_gpio_adc_disable(
        struct whitebox_platform_data_t* platform_data);

void whitebox_gpio_cmx991_reset(
        struct whitebox_platform_data_t* platform_data);
void whitebox_gpio_cmx991_write(
        struct whitebox_platform_data_t* platform_data,
        u8 address,
        u8 data);
u8 whitebox_gpio_cmx991_read(
        struct whitebox_platform_data_t* platform_data,
        u8 address);
void whitebox_gpio_adf4351_write(
        struct whitebox_platform_data_t* platform_data,
        u32 data);
int whitebox_gpio_adf4351_locked(
        struct whitebox_platform_data_t* platform_data);
void whitebox_gpio_adf4360_write(
        struct whitebox_platform_data_t* platform_data,
        u32 data);
int whitebox_gpio_adf4360_locked(
        struct whitebox_platform_data_t* platform_data);

// Turn on the sampling clock and analog regulators
int whitebox_gpio_analog_enable(
    struct whitebox_platform_data_t* platform_data);
// Turn off the sampling clock and analog regulators
int whitebox_gpio_analog_disable(
    struct whitebox_platform_data_t* platform_data);
// Control the gateway
int whitebox_gpio_gateway_control(
    struct whitebox_platform_data_t *platform_data,
    u8 transmit, // 1 - transmit, 0 - receive
    u8 amplifier, // PA (LNA separate control in CMX991)
    u8 detect, // Switch in TX calibration log detector
    u8 noise, // Switch in receiving noise for RX self-test
    u8 bpf); // Which bandpass filter to use; 0, 1, 2, 3, 4

#endif /* __WHITEBOX_GPIO_H */
