"""
Direct Digital Synthesizer
==========================
"""
from math import cos, pi, ceil
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
    pa_bitwidth = kwargs.get('phase_accumulator_bitwidth', 40)
    lut_bitwidth = kwargs.get('lut_bitwidth', 10)
    num_samples = kwargs.get('num_samples', 1024)
    sample_resolution = len(output)
    half = pow(2, sample_resolution - 1)
    samples = tuple([int(ceil(cos(i)*(half-1))) \
                for i in frange(0, 2*pi, step=(2*pi)/num_samples)])

    phase_accumulator = Signal(modbv(0)[pa_bitwidth:])
    
    @always_seq(clock.posedge, reset=resetn)
    def synthesizer():
        if enable:
            phase_accumulator.next = phase_accumulator + frequency_control_word
            output.next = samples[phase_accumulator[
                pa_bitwidth:pa_bitwidth-lut_bitwidth+1]]

    return synthesizer

