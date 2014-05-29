#include <linux/ioctl.h>

// DAC Rate
#define W_DAC_RATE_HZ   6144000UL

// DDS Constants
#define W_DDS_PA_BITWIDTH 25
#define W_DDS_PA_COUNT    (1 << W_DDS_PA_BITWIDTH)

// Number of registers for the CMX991
#define WC_REGS_COUNT   10U

// Number of registers for the ADF4351
#define WA_REGS_COUNT   6U

// Maximum number of coefficients that can be in the FIR
#define WF_COEFF_COUNT 128

typedef struct whitebox_args {
    union {
        struct {
            uint32_t state;
            uint32_t interp;
            uint32_t fcw;
            uint32_t runs;
            uint32_t threshold;
            uint32_t correction;
            uint32_t available;
            uint32_t debug;
            uint32_t gain;
        } exciter;
        struct {
            uint32_t state;
            uint32_t decim;
            uint32_t fcw;
            uint32_t runs;
            uint32_t threshold;
            uint32_t correction;
            uint32_t available;
            uint32_t debug;
        } receiver;
        uint8_t cmx991[WC_REGS_COUNT];
        uint32_t adf4351[WA_REGS_COUNT];
        struct {
            uint8_t bank;
            uint8_t n;
            int32_t coeff[WF_COEFF_COUNT];
        } fir;
    } flags;
    uint8_t locked;
    uint32_t mock_command;
} whitebox_args_t;

// Location of the register files
#define WHITEBOX_EXCITER_REGS          0x40050400
#define WHITEBOX_RECEIVER_REGS         0x40050480

/* Register File */
#define WE_SAMPLE_ADDR          0x0
#define WE_STATUS_ADDR          0x4
#define WE_INTERP_ADDR          0x8
#define WE_FCW_ADDR             0xc
#define WE_RUNS_ADDR            0x10
#define WE_THRESHOLD_ADDR       0x14
#define WE_CORRECTION_ADDR      0x18
#define WE_AVAILABLE_ADDR       0x1c
#define WE_DEBUG_ADDR           0x20
#define WE_GAIN_ADDR            0x24
#define W_FIR_ADDR              0x28
#define WR_SAMPLE_ADDR          0x30
#define WR_STATUS_ADDR          0x34
#define WR_DECIM_ADDR           0x38
#define WR_RUNS_ADDR            0x40
#define WR_THRESHOLD_ADDR       0x44
#define WR_CORRECTION_ADDR      0x48
#define WR_AVAILABLE_ADDR       0x4c
#define WR_DEBUG_ADDR           0xa0

/* Status Flags */
#define WS_CLEAR                (1 << 0)
#define WS_LOOPEN               (1 << 1)
#define WES_TXEN                (1 << 8)
#define WES_FILTEREN            (1 << 9)
#define WES_DDSEN               (1 << 10)
#define WES_TXSTOP              (1 << 11)
#define WES_AEMPTY              (1 << 12)
#define WES_AFULL               (1 << 13)
#define WES_SPACE               (1 << 14)
#define WES_DATA                (1 << 15)
#define WRS_RXEN                (1 << 16)
#define WRS_FILTEREN            (1 << 17)
#define WRS_RXSTOP              (1 << 19)
#define WRS_AEMPTY              (1 << 20)
#define WRS_AFULL               (1 << 21)
#define WRS_SPACE               (1 << 22)
#define WRS_DATA                (1 << 23)
#define WS_FIREN                (1 << 24)
#define WF_ACCESS_COEFFS        (1 << 16)

/* General */
#define W_RESET _IO('w', 1)
#define W_LOCKED _IO('w', 2)

/* Exciter */
#define WE_CLEAR _IO('w', 3)
#define WE_GET  _IOR('w', 4, whitebox_args_t*)
#define WE_SET _IOW('w', 5, whitebox_args_t*)
#define WE_CLEAR_MASK _IOW('w', 20, whitebox_args_t*)

#define WE_FIFO_SIZE    1024L

#define WER_OVERRUNS_MASK    0xffff0000
#define WER_OVERRUNS_OFFSET  16
#define WER_UNDERRUNS_MASK   0x0000ffff

#define WET_AFVAL_MASK       0xffff0000
#define WET_AFVAL_OFFSET     16
#define WET_AEVAL_MASK       0x0000ffff

#define WEC_I_MASK           0x000003ff
#define WEC_Q_OFFSET         16
#define WEC_Q_MASK           0x03ff0000

#define WEG_COEFF            ((float)(1 << 9))
#define WEG_I_MASK           0x000003ff
#define WEG_Q_OFFSET         16
#define WEG_Q_MASK           0x03ff0000

/* Receiver */
#define WR_CLEAR _IO('w', 6)
#define WR_GET  _IOR('w', 7, whitebox_args_t*)
#define WR_SET _IOW('w', 8, whitebox_args_t*)

#define WR_FIFO_SIZE    1024

#define WRR_OVERRUNS_MASK    0xffff0000
#define WRR_OVERRUNS_OFFSET  16
#define WRR_UNDERRUNS_MASK   0x0000ffff

#define WRT_AFVAL_MASK       0xffff0000
#define WRT_AFVAL_OFFSET     16
#define WRT_AEVAL_MASK       0x0000ffff

/* CMX991 */
#define WC_GET _IOR('w', 9, whitebox_args_t*)
#define WC_SET _IOW('w', 10, whitebox_args_t*)
#define WC_LOCKED _IO('w', 17)

// Whether or not to use the CMX991 PLL
#define WC_USE_PLL      1

/* ADF4351 */
#define WA_GET _IOR('w', 11, whitebox_args_t*)
#define WA_SET _IOW('w', 12, whitebox_args_t*)
#define WA_LOCKED _IO('w', 16)

#define WA_CLOCK_RATE 26.0e6
#define WA_PD_MASK   0x00000020

/* Mock Commands */
#define WM_CMD _IOW('w', 13, whitebox_args_t*)

#define WMC_CAUSE_UNDERRUN      (1 << 0)
#define WMC_CAUSE_OVERRUN       (1 << 1)

/* Zero-memory copy from user space */
#define W_MMAP_WRITE _IOR('w', 14, unsigned long*)
#define W_MMAP_READ  _IOR('w', 15, unsigned long*)

/* FIR Filter */
#define WF_GET _IOR('w', 18, whitebox_args_t*)
#define WF_SET _IOR('w', 19, whitebox_args_t*)
