#ifndef __PDMA_H__
#define __PMDA_H__

#define PDMA_ROUND_ROBIN 0
#define PDMA_RATIO_HIGH_LOW_1_TO_1 1
#define PDMA_RATIO_HIGH_LOW_3_TO_1 3
#define PDMA_RATIO_HIGH_LOW_7_TO_1 7
#define PDMA_RATIO_HIGH_LOW_15_TO_1 15
#define PDMA_RATIO_HIGH_LOW_31_TO_1 31
#define PDMA_RATIO_HIGH_LOW_63_TO_1 63
#define PDMA_RATIO_HIGH_LOW_127_TO_1 127
#define PDMA_RATIO_HIGH_LOW_255_TO_1 255

#define PDMA_CONTROL_PER_SEL_UART0_RX    (0x0 << 23)
#define PDMA_CONTROL_PER_SEL_UART0_TX    (0x1 << 23)
#define PDMA_CONTROL_PER_SEL_UART1_RX    (0x2 << 23)
#define PDMA_CONTROL_PER_SEL_UART1_TX    (0x3 << 23)
#define PDMA_CONTROL_PER_SEL_SPI0_RX     (0x4 << 23)
#define PDMA_CONTROL_PER_SEL_SPI0_TX     (0x5 << 23)
#define PDMA_CONTROL_PER_SEL_SPI1_RX     (0x6 << 23)
#define PDMA_CONTROL_PER_SEL_SPI1_TX     (0x7 << 23)
#define PDMA_CONTROL_PER_SEL_FPGA1       (0x8 << 23)
#define PDMA_CONTROL_PER_SEL_FPGA0       (0x9 << 23)
#define PDMA_CONTROL_PER_SEL_ACE_RX      (0x10 << 23)
#define PDMA_CONTROL_PER_SEL_ACE_TX      (0x11 << 23)

#define PDMA_CONTROL_DST_ADDR_INC_0 (0x0 << 12)
#define PDMA_CONTROL_DST_ADDR_INC_1 (0x1 << 12)
#define PDMA_CONTROL_DST_ADDR_INC_2 (0x2 << 12)
#define PDMA_CONTROL_DST_ADDR_INC_4 (0x3 << 12)
#define PDMA_CONTROL_SRC_ADDR_INC_0 (0x0 << 10)
#define PDMA_CONTROL_SRC_ADDR_INC_1 (0x1 << 10)
#define PDMA_CONTROL_SRC_ADDR_INC_2 (0x2 << 10)
#define PDMA_CONTROL_SRC_ADDR_INC_4 (0x3 << 10)

#define PDMA_CONTROL_LOW_PRIORITY   (0x0 << 9)
#define PDMA_CONTROL_HIGH_PRIORITY  (0x1 << 9)

#define PDMA_CONTROL_CLR_B      (1 << 8)
#define PDMA_CONTROL_CLR_A      (1 << 7)
#define PDMA_CONTROL_INTEN      (1 << 6)
#define PDMA_CONTROL_RESET      (1 << 5)
#define PDMA_CONTROL_PAUSE      (1 << 4)

#define PDMA_CONTROL_XFER_SIZE_1B   (0x0 << 2)
#define PDMA_CONTROL_XFER_SIZE_2B   (0x1 << 2)
#define PDMA_CONTROL_XFER_SIZE_4B   (0x2 << 2)

#define PDMA_CONTROL_DIR_PERIPH_TO_MEM  (0 << 1)
#define PDMA_CONTROL_DIR_MEM_TO_PERIPH  (1 << 1)

#define PDMA_CONTROL_MEM        (0 << 0)
#define PDMA_CONTROL_PERIPH     (1 << 0)

#define PDMA_STATUS_BUF_SEL_B   (1 << 2)
#define PDMA_STATUS_CH_COMP_B   (1 << 1)
#define PDMA_STATUS_CH_COMP_A   (1 << 0)

typedef void (*pdma_irq_handler_t)(void* data, int buf);

int pdma_set_priority_ratio(int ratio);

int pdma_request(u8 ch, pdma_irq_handler_t handler, void* user_data, u8 write_adj, u32 flags);

void pdma_clear(u8 ch);
void pdma_release(u8 ch);

int pdma_start(u8 ch, u32 src, u32 dst, u16 cnt);

int pdma_buffers_available(u8 ch);

#endif /* __PDMA_H__ */
