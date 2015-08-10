#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <mach/m2s.h>
#include <mach/clock.h>
#include "whitebox.h"

#define SPI1_M2S_ID		1
#define SPI1_M2S_REGS		0x40011000
#define SPI1_M2S_CLK		CLCK_PCLK1
#define SPI1_M2S_RX_DMA		2
#define SPI1_M2S_TX_DMA		3

struct spi_m2s_platform_data {
    unsigned int    ref_clk;    /* Reference clock  */
    unsigned char   dma_rx;     /* Rx DMA channel   */
    unsigned char   dma_tx;     /* Tx DMA channel   */
};

static struct resource spi_m2s_dev1_resources[] = {
	{
		.start	= SPI1_M2S_REGS,
		.end	= SPI1_M2S_REGS + 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct spi_m2s_platform_data spi_m2s_dev1_data = {
	.dma_rx		= SPI1_M2S_RX_DMA,
	.dma_tx		= SPI1_M2S_TX_DMA,
};

static struct platform_device spi_m2s_dev1 = {
	.name = "spi_m2s",
	.id	= SPI1_M2S_ID,
	.num_resources = ARRAY_SIZE(spi_m2s_dev1_resources),
	.resource = spi_m2s_dev1_resources,
};

static struct spi_board_info auxcodec_spi_board_info[] = {
  {
    .modalias = "spidev",
    .mode = SPI_MODE_3,
    .max_speed_hz = 8000000,
    .bus_num = 1,
    .chip_select = 2,
  },
  /*{
    .modalias = "dac7565",
    .mode = SPI_MODE_0,
    .max_speed_hz = 50000000,
    .bus_num = 1,
    .chip_select = 3,
  },*/
};

int whitebox_spi_init(struct whitebox_device *wb) {
	spi_m2s_dev1_data.ref_clk = m2s_clock_get(SPI1_M2S_CLK);
	platform_set_drvdata(&spi_m2s_dev1, &spi_m2s_dev1_data);
	platform_device_register(&spi_m2s_dev1);
    wb->spi1_master = spi_busnum_to_master(SPI1_M2S_ID);
    wb->auxcodec_spi_device = spi_new_device(wb->spi1_master,
        auxcodec_spi_board_info);
    wb->auxcodec_spi_device->bits_per_word = 16;
    return wb->auxcodec_spi_device == NULL ? -1 : 0;
}

int whitebox_spi_auxadc_get(struct whitebox_device *wb, int16_t* values) {
    int result, i;
    struct spi_message msg;
    struct spi_transfer xfer;
    int16_t tx_buf[] = { 0 << 11, 1 << 11, 2 << 11, 3 << 11,
                         4 << 11, 5 << 11, 6 << 11, 7 << 11 };
    printk(KERN_INFO "auxadc_get %d\n", sizeof(tx_buf));

    spi_message_init(&msg);
    memset(&xfer, 0, sizeof xfer);
    xfer.tx_buf = tx_buf;
    xfer.rx_buf = values;
    xfer.bits_per_word = 16;
    xfer.len = sizeof(tx_buf);
    spi_message_add_tail(&xfer, &msg);
    result = spi_sync(wb->auxcodec_spi_device, &msg);
    for (i = 0; i < 8; ++i)
        printk(KERN_INFO "%d %d\n", i, values[i]);
    return result;
}

// TODO: release master with spi_master_put()
