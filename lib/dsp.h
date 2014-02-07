#ifndef __WHITEBOX_DSP_H__
#define __WHITEBOX_DSP_H__

#define DDS_PA_LENGTH            25
#define DDS_PA_MAX               (1 << DDS_PA_LENGTH)
#define DDS_ROM_SAMPLES_ORDER    10
#define DDS_ROM_NUM_SAMPLES      (1 << DDS_ROM_SAMPLES_ORDER)
#define DDS_ROM_DEPTH            16
#define DDS_PHASE_SHIFT          (DDS_PA_LENGTH - DDS_ROM_SAMPLES_ORDER)

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
 * Generates a list of complex interleaved samples from a list of phases.
 *
 * c[i] = cos(phase) + 1j * sin(phase)
 */
uint32_t sincos16c(int n, uint32_t fcw, uint32_t p0, uint32_t *c);

#endif /* __WHITEBOX_DSP_H__ */
