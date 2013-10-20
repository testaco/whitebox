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

#endif /* __WHITEBOX_GPIO_H */
