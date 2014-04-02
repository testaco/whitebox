#ifndef __WHITEBOX_TX__
#define __WHITEBOX_TX__
#include <linux/interrupt.h>

#include "whitebox.h"

void tx_init(struct whitebox_device *wb);
int tx_start(struct whitebox_device *wb);
int tx_exec(struct whitebox_device* wb, int call_type);
void tx_dma_cb(void *data);
void tx_stop(struct whitebox_device *wb);
int tx_error(struct whitebox_device *wb);
void tx_tasklet_exec(unsigned long data);
irqreturn_t tx_irq_cb(int irq, void *data);

#define TX_EXEC_BY_WRITE    0
#define TX_EXEC_BY_DMA      1
#define TX_EXEC_BY_IRQ      2
#define TX_EXEC_BY_TIMER    3

#endif /* __WHITEBOX_TX__ */
