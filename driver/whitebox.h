#ifndef __WHITEBOX_H
#define __WHITEBOX_H

#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/cdev.h>

#include "whitebox_ioctl.h"
#include "whitebox_block.h"
#include "whitebox_exciter.h"
#include "whitebox_receiver.h"

#define WHITEBOX_DRIVER_NAME "whitebox"

#define WHITEBOX_VERBOSE_DEBUG 0

#define WHITEBOX_EXCITER_SAMPLE_OFFSET 0x00
#define WHITEBOX_EXCITER_STATUS_OFFSET 0x04
#define WHITEBOX_EXCITER_INTERP_OFFSET 0x08
#define WHITEBOX_EXCITER_FCW_OFFSET    0x0c
#define WHITEBOX_EXCITER_REGS_COUNT    0x10

#define WHITEBOX_RECEIVER_SAMPLE_OFFSET 0x00
#define WHITEBOX_RECEIVER_STATUS_OFFSET 0x04
#define WHITEBOX_RECEIVER_INTERP_OFFSET 0x08
#define WHITEBOX_RECEIVER_FCW_OFFSET    0x0c
#define WHITEBOX_RECEIVER_REGS_COUNT    0x10

#define WHITEBOX_CMX991_REGS_READ_BASE  0xe1
#define WHITEBOX_CMX991_REGS_WRITE_BASE 0x11
#define WHITEBOX_CMX991_LD_REG          0xd1
#define WHITEBOX_CMX991_LD_MASK         0x40

/*
 * These whitebox pin to Linux kernel GPIO mappings are derived from the
 * Whitebox Libero SmartDesign.  */
#define FPGA_GPIO_BASE 32
#define WHITEBOX_GPIO_ADC_S2       (FPGA_GPIO_BASE+3)
#define WHITEBOX_GPIO_ADC_S1       (FPGA_GPIO_BASE+4)
#define WHITEBOX_GPIO_ADC_DFS      (FPGA_GPIO_BASE+5)
#define WHITEBOX_GPIO_DAC_EN       (FPGA_GPIO_BASE+6)
#define WHITEBOX_GPIO_DAC_PD       (FPGA_GPIO_BASE+7)
#define WHITEBOX_GPIO_DAC_CS       (FPGA_GPIO_BASE+8)
#define WHITEBOX_GPIO_RADIO_RESETN (FPGA_GPIO_BASE+9)
#define WHITEBOX_GPIO_RADIO_CDATA  (FPGA_GPIO_BASE+10)
#define WHITEBOX_GPIO_RADIO_SCLK   (FPGA_GPIO_BASE+11)
#define WHITEBOX_GPIO_RADIO_RDATA  (FPGA_GPIO_BASE+12)
#define WHITEBOX_GPIO_RADIO_CSN    (FPGA_GPIO_BASE+13)
#define WHITEBOX_GPIO_VCO_CLK      (FPGA_GPIO_BASE+14)
#define WHITEBOX_GPIO_VCO_DATA     (FPGA_GPIO_BASE+15)
#define WHITEBOX_GPIO_VCO_LE       (FPGA_GPIO_BASE+16)
#define WHITEBOX_GPIO_VCO_CE       (FPGA_GPIO_BASE+17)
#define WHITEBOX_GPIO_VCO_PDB      (FPGA_GPIO_BASE+18)
#define WHITEBOX_GPIO_VCO_LD       (FPGA_GPIO_BASE+19)

enum whitebox_device_state {
    WDS_IDLE,
    WDS_TX,
    WDS_TX_STREAMING,
    WDS_TX_STOPPING,
    WDS_RX,
    WDS_RX_STREAMING,
    WDS_RX_STOPPING,
};

#define W_ERROR_PLL_LOCK_LOST 1
#define W_ERROR_TX_OVERRUN    2
#define W_ERROR_TX_UNDERRUN   3
#define W_ERROR_RX_OVERRUN    2
#define W_ERROR_RX_UNDERRUN   3

/* Must be a power of two. */
#define W_EXEC_DETAIL_COUNT    (1 << 6)

struct whitebox_stats_exec_detail {
    int time;
    size_t src;
    size_t dest;
    int bytes;
    int result;
};

struct whitebox_stats {
    long bytes;
    long exec_calls;
    long exec_busy;
    long exec_nop_src;
    long exec_nop_dest;
    long exec_failed;
    long exec_success_slow;
    long exec_dma_start;
    long exec_dma_finished;
    struct whitebox_stats_exec_detail exec_detail[W_EXEC_DETAIL_COUNT];
    int exec_detail_index;
    long stop;
    long error;
    int last_error;
};

struct spi_device;
struct spi_master;

/*
 * Book-keeping for the device
 */
struct whitebox_device {
    struct semaphore sem;
    struct cdev cdev;
    struct device* device;
    struct dentry *debugfs_root;
    enum whitebox_device_state state;
    int irq;
    int irq_disabled;
    struct whitebox_platform_data_t* platform_data;
    wait_queue_head_t write_wait_queue;
    wait_queue_head_t read_wait_queue;
    atomic_t mapped;

    u32 adf4351_regs[WA_REGS_COUNT];
    u32 adf4360_regs[WA60_REGS_COUNT];
    u8 cmx991_regs[WC_REGS_COUNT];
    u16 cur_overruns, cur_underruns;

    struct circ_buf mock_buf;
    struct whitebox_mock_exciter mock_exciter;
    struct whitebox_mock_receiver mock_receiver;

    struct whitebox_exciter exciter;
    struct whitebox_receiver receiver;

    unsigned long user_buffer;
    struct whitebox_user_source user_source;
    struct whitebox_user_sink user_sink;

    struct whitebox_rf_source rf_source;
    struct whitebox_rf_sink rf_sink;

    struct whitebox_stats tx_stats;
    struct whitebox_stats rx_stats;
    struct {
        u8 transmit;
        u8 amplifier;
        u8 detect;
        u8 noise;
        u8 bpf;
    } gateway;

    struct spi_master* spi1_master;
    struct spi_device* auxcodec_spi_device;

    struct {
        s16 rssi;
        s16 det;
        s16 ifmuxout;
        s16 rfmuxout;
        s16 xcvr_temp;
        s16 vsense;
    } auxcodec;
};

/*
 * Platform data includes pin mappings for each GPIO on the Bravo card
 */
struct whitebox_platform_data_t {
    // Power Control
    unsigned en_pin;
    unsigned pd_pin;

    // RF SPI Select
    unsigned radio_cs_pin; // ce1
    unsigned rflo_cs_pin;  // ce2
    unsigned iflo_cs_pin;  // ce3
    unsigned gw_cs_pin;    // ce4

    // RF SPI
    unsigned mosi_pin;
    unsigned sclk_pin;
    unsigned miso_pin;

    // Lock Detect
    unsigned rfld_pin;

    // CODEC
    unsigned adc_s1_pin;
    unsigned adc_s2_pin;
    unsigned adc_dfs_pin;
    unsigned dac_pd_pin;
    unsigned dac_cs_pin;

    // Gateway
    unsigned detect_pin;
    unsigned amp_pin;
    unsigned tr_pin;
    unsigned noise_pin;

    // Input-Output
    unsigned ptt_out_pin;
    unsigned ptt_in_pin;

    // DMA
    u8 tx_dma_ch;
    u8 rx_dma_ch;
};

#define WHITEBOX_PLATFORM_DATA(pdev) ((struct whitebox_platform_data_t *)(pdev->dev.platform_data))

extern int whitebox_check_plls;
extern int whitebox_check_runs;
extern int whitebox_loopen;
extern int whitebox_debug;
extern int whitebox_flow_control;
extern int whitebox_frame_size;
extern int whitebox_user_source_buffer_threshold;
extern int whitebox_user_sink_buffer_threshold;

#endif /* __WHITEBOX_H */
