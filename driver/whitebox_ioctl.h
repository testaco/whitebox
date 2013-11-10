#include <linux/ioctl.h>

// DAC Rate
#define W_DAC_RATE_HZ   10000000

// Number of registers for the CMX991
#define WC_REGS_COUNT   10U

// Number of registers for the ADF4351
#define WA_REGS_COUNT   6U

typedef struct whitebox_args {
    union {
        struct {
            uint32_t state;
            uint32_t interp;
            uint32_t fcw;
            uint32_t runs;
            uint32_t threshold;
        } exciter;
        uint8_t cmx991[WC_REGS_COUNT];
        uint32_t adf4351[WA_REGS_COUNT];
    } flags;
    uint8_t locked;
    uint32_t mock_command;
} whitebox_args_t;

#define W_RESET _IO('w', 1)
#define W_LOCKED _IOR('w', 2, whitebox_args_t*)

#define WE_CLEAR _IO('w', 3)
#define WE_GET  _IOR('w', 4, whitebox_args_t*)
#define WE_SET _IOW('w', 5, whitebox_args_t*)

#define WE_FIFO_SIZE    1024

#define WES_CLEAR       0x00000001
#define WES_TXSTOP      0x00000002
#define WES_TXEN        0x00000100
#define WES_DDSEN       0x00000200
#define WES_FILTEREN    0x00000400
#define WES_AEMPTY      0x00010000
#define WES_AFULL       0x00020000
#define WES_SPACE       0x00100000
#define WES_DATA        0x00200000

#define WER_OVERRUNS_MASK    0xffff0000
#define WER_OVERRUNS_OFFSET  16
#define WER_UNDERRUNS_MASK   0x0000ffff

#define WET_AFVAL_MASK       0xffff0000
#define WET_AFVAL_OFFSET     16
#define WET_AEVAL_MASK       0x0000ffff

#define WC_GET _IOR('w', 6, whitebox_args_t*)
#define WC_SET _IOW('w', 7, whitebox_args_t*)

#define WA_GET _IOR('w', 8, whitebox_args_t*)
#define WA_SET _IOW('w', 9, whitebox_args_t*)

#define WM_CMD _IOW('w', 10, whitebox_args_t*)

#define WMC_CAUSE_UNDERRUN    (1 << 0)
