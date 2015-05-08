"""
"""
from math import cos, pi, ceil, log, sin
from myhdl import Signal, always, always_seq, intbv, modbv, always_comb, concat

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
    fmin = fcw_to_freq(1, **kwargs)
    fmax = fcw_to_freq((2 ** pa_bitwidth) - 1, **kwargs)
    if freq < fmin or freq > fmax:
        raise AttributeError, "Frequency is outside of range %d to %d" % (fmin, fmax)
    return int(freq / (sample_rate / pa_cnt))

def dds_lut(sample_resolution, num_samples, scale_factor):
    half = scale_factor*pow(2, sample_resolution - 1)
    i_samples = tuple([int(ceil(cos(i)*(half-1))) \
                for i in frange(0, 2*pi, step=(2*pi)/num_samples)])
    q_samples = tuple([int(ceil(sin(i)*(half-1))) \
                for i in frange(0, 2*pi, step=(2*pi)/num_samples)])
    return i_samples, q_samples

def dds(resetn,
        clock,
        enable,
        in_,
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

    in_valid = in_.valid
    in_last = in_.last
    in_i = in_.i
    in_q = in_.q

    pa_bitwidth = kwargs.get('phase_accumulator_bitwidth', 25)
    lut_bitwidth = kwargs.get('lut_bitwidth', len(output_i))
    num_samples = kwargs.get('num_samples', 1024)
    lgsamples = int(ceil(log(num_samples, 2)))
    sample_resolution = len(output_i)
    scale_factor = 0.8  # To compensate for the DC Offset Correction
    i_samples, q_samples = dds_lut(sample_resolution, num_samples, scale_factor)

    phase_accumulator = Signal(modbv(0)[pa_bitwidth:])
    phase_accumulator_q = Signal(modbv(0)[pa_bitwidth:])

    fcw_bitwidth = len(frequency_control_word)
    fcw = Signal(intbv(0, min=0, max=2**fcw_bitwidth))
    fcw_tmp = Signal(intbv(0, min=0, max=2**fcw_bitwidth))
    
    e = Signal(bool(0))
    e_tmp = Signal(bool(0))

    i_nco = Signal(intbv(0, min=-2**(lgsamples), max=2**(lgsamples)))
    q_nco = Signal(intbv(0, min=-2**(lgsamples), max=2**(lgsamples)))

    @always_comb
    def phase_shift():
        phase_accumulator_q.next = phase_accumulator - 2 ** (pa_bitwidth - 2)
        #phase_accumulator_q.next = phase_accumulator

    @always_comb
    def lut():
        i_nco.next = i_samples[phase_accumulator[
                pa_bitwidth:pa_bitwidth-lgsamples]]
        q_nco.next = i_samples[phase_accumulator_q[
                pa_bitwidth:pa_bitwidth-lgsamples]]

    @always_seq(clock.posedge, reset=resetn)
    def synchronizer():
        fcw_tmp.next = frequency_control_word
        fcw.next = fcw_tmp
        e_tmp.next = enable
        e.next = e_tmp

    s = len(i_nco) + len(in_i) - 1
    mixed_i = Signal(intbv(0, min=-2**s, max=2**s))
    mixed_q = Signal(intbv(0, min=-2**s, max=2**s))
    mixed_valid = Signal(bool(False))
    mixed_last = Signal(bool(False))

    @always_seq(clock.posedge, reset=resetn)
    def mix():
        if e:
            phase_accumulator.next = phase_accumulator + fcw
            mixed_i.next = i_nco.signed() * in_i.signed()
            mixed_q.next = q_nco.signed() * in_q.signed()
            mixed_valid.next = in_valid
            mixed_last.next = in_last
        else:
            mixed_valid.next = False
            output_i.next = in_i
            output_q.next = in_q
            output_valid.next = in_valid
            output_last.next = in_last

        if mixed_valid:
            output_valid.next = mixed_valid
            output_last.next = mixed_last
            output_i.next = mixed_i[len(mixed_i)-1:len(mixed_i)-len(output_i)-1].signed()
            output_q.next = mixed_q[len(mixed_q)-1:len(mixed_q)-len(output_q)-1].signed()
        else:
            output_i.next = in_i
            output_q.next = in_q
            output_valid.next = in_valid
            output_last.next = in_last


    return phase_shift, lut, synchronizer, mix
