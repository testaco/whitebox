#ifndef __WHITEBOX_DSP_H__
#define __WHITEBOX_DSP_H__

#define DDS_PA_LENGTH            25U
#define DDS_PA_MAX               (1U << DDS_PA_LENGTH)
#define DDS_ROM_SAMPLES_ORDER    10U
#define DDS_ROM_NUM_SAMPLES      (1U << DDS_ROM_SAMPLES_ORDER)
#define DDS_ROM_DEPTH            32U
#define DDS_RAM_SIZE_BITS        (DDS_ROM_NUM_SAMPLES * DDS_ROM_DEPTH)
#define DDS_PHASE_SHIFT          (DDS_PA_LENGTH - DDS_ROM_SAMPLES_ORDER)

#define DDS_LUT_ADDR         0x20001000

#define QUAD_PACK(i, q) ((uint32_t)(i) & 0xffffUL) | (((uint32_t)(q) & 0xffffUL) << 16U)

#define QUAD_UNPACK(s, i, q) { \
    i = (int16_t)((s) & 0xffffUL); \
    q = (int16_t)(((s) >> 16) & 0xffffUL); \
    }


/*
 * Figures out the frequency control word for a given frequency and sample
 * rate.
 */
uint32_t freq_to_fcw(float freq, float sample_rate);

/*
 * Creates a linear phase space of n samples, starting at p0 and incrementing
 * by the frequency control word.
 *
 * phases[0] = p0
 * phases[i] = phases[i-1] + fcw
 */
void accum32(int n, uint32_t fcw, uint32_t p0, uint32_t *phases);

/*
 * Computes the cos (i) and sin (q) given the associated phase with the DDS.
 */
void sincos16(uint32_t phase, int16_t *i, int16_t *q);

/*
 * ret = cos(phase) + 1j * sin(phase)
 */
uint32_t sincos16c(uint32_t fcw, uint32_t *phase);

extern uint32_t *sincos_lut_addr;

#endif /* __WHITEBOX_DSP_H__ */
