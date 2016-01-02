"""
"""
from math import cos, pi, ceil, log, sin
from myhdl import Signal, always, always_seq, intbv, modbv

# Reference: http://www.daniweb.com/software-development/python/code/227432/a-floating-point-range-genertaor
def frange(start, stop=None, step=1.0, delta=0.0000001):
    if stop == None:
        stop = start
        start = 0.0
    if step <= 0:
        while start > (stop + delta):
            yield start
            start += step
    else:
        while start < (stop - delta):
            yield start
            start += step

def fcw_to_freq(fcw, **kwargs):
    sample_rate = kwargs.get('sample_rate', 10e6)
    pa_bitwidth = kwargs.get('phase_accumulator_bitwidth', 25)
    pa_cnt = 2 ** pa_bitwidth
    return fcw * (sample_rate / pa_cnt)

def freq_to_fcw(freq, **kwargs):
    sample_rate = kwargs.get('sample_rate', 10e6)
    pa_bitwidth = kwargs.get('phase_accumulator_bitwidth', 25)
    pa_cnt = 2 ** pa_bitwidth
    fmin = fcw_to_freq(0, **kwargs)
    fmax = fcw_to_freq((2 ** pa_bitwidth) - 1, **kwargs)
    if freq < fmin or freq > fmax:
        raise AttributeError, "Frequency is outside of range %d to %d" % (fmin, fmax)
    return int(freq / (sample_rate / pa_cnt))

def dds_lut(sample_resolution, num_samples, scale_factor):
    half = scale_factor*pow(2, sample_resolution - 1)
    i_samples = tuple([int(ceil(cos(i)*(half-1))) \
                for i in frange(0, 2*pi, step=(2*pi)/num_samples)])
    q_samples = tuple([int(ceil(-sin(i)*(half-1))) \
                for i in frange(0, 2*pi, step=(2*pi)/num_samples)])
    return i_samples, q_samples

def dds(resetn,
        clock,
        enable,
        output,
        frequency_control_word,
        **kwargs):
    """Synthesizable DDS using a LUT.

    :param resetn: Reset.
    :param clock: Driving clock for the DDS.
    :param output: The output digital control word.
    :param frequency_control_word: The fcw.
    """
    output_valid = output.valid
    output_last = output.last
    output_i = output.i
    output_q = output.q

    pa_bitwidth = kwargs.get('phase_accumulator_bitwidth', 25)
    lut_bitwidth = kwargs.get('lut_bitwidth', 10)
    num_samples = kwargs.get('num_samples', 1024)
    lgsamples = int(ceil(log(num_samples, 2)))
    sample_resolution = len(output_i)
    scale_factor = 0.8  # To compensate for the DC Offset Correction
    i_samples, q_samples = dds_lut(sample_resolution, num_samples, scale_factor)

    phase_accumulator = Signal(modbv(0)[pa_bitwidth:])

    fcw_bitwidth = len(frequency_control_word)
    fcw = Signal(intbv(0, min=0, max=2**fcw_bitwidth))
    fcw_tmp = Signal(intbv(0, min=0, max=2**fcw_bitwidth))
    
    e = Signal(bool(0))
    e_tmp = Signal(bool(0))

    @always_seq(clock.posedge, reset=resetn)
    def synthesizer():
        fcw_tmp.next = frequency_control_word
        fcw.next = fcw_tmp
        e_tmp.next = enable
        e.next = e_tmp

        if e and output.ready:
            phase_accumulator.next = phase_accumulator + fcw
            output_i.next = i_samples[phase_accumulator[
                pa_bitwidth:pa_bitwidth-lgsamples]]
            output_q.next = q_samples[phase_accumulator[
                pa_bitwidth:pa_bitwidth-lgsamples]]
            output_valid.next = 1
            output_last.next = 0
        else:
            output_i.next = 0
            output_q.next = 0
            output_valid.next = 0
            output_last.next = 0

    return synthesizer
