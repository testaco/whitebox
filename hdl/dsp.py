import math
import numpy as np
from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

SIGNATURE_SIGNED = True
SIGNATURE_UNSIGNED = False

class Signature(object):
    def __init__(self, name, signed, **kwargs):
        self.name = name
        self.signed = signed

        if 'bits' in kwargs:
            bits = self.bits = kwargs['bits']
            if signed:
                Q = 2**(bits-1)
                self.min = -Q
                self.max = Q
            else:
                Q = 2**bits
                self.min = 0
                self.max = Q

        elif 'min' in kwargs and 'max' in kwargs:
            self.min = kwargs['min']
            self.max = kwargs['max']
            self.bits = len(self.myhdl(0))

        else:
            raise AttributeError, "Must give bits or min & max"

        self.valid = kwargs.get('valid', Signal(bool(0)))
        self.last = kwargs.get('last', Signal(bool(0)))
        self.i = kwargs.get('i', Signal(intbv(0, min=self.min, max=self.max)))
        self.q = kwargs.get('q', Signal(intbv(0, min=self.min, max=self.max)))

    def __repr__(self):
        return '<Signature name=%s bits=%d min=%d max=%d>' % (self.name,
                self.bits, self.min, self.max)

    def myhdl(self, default):
        return intbv(int(default), min=self.min, max=self.max)

    def myhdl_overflow_acceptable(self, default):
        return modbv(int(default), min=self.min, max=self.max)

    def numpy_dtype(self):
        if self.signed:
            if self.bits <= 16:
                return np.int16
            elif self.bits <= 32:
                return np.int32
            elif self.bits <= 64:
                return np.int64
            else:
                raise AttributeError, 'Too many bits!'
        else:
            if self.bits <= 16:
                return np.uint16
            elif self.bits <= 32:
                return np.uint32
            elif self.bits <= 64:
                return np.uint64
            else:
                raise AttributeError, 'Too many bits!'

    def copy(self, name):
        return Signature(name, self.signed, bits=self.bits)

    def copy_with_bit_gain(self, name, bit_gain):
        new_bits = self.bits + bit_gain
        return Signature(name, self.signed, bits=new_bits)

    def copy_with_gain(self, name, gain):
        bit_gain = math.ceil(math.log(gain, 2))
        return self.copy_with_bit_gain(name, bit_gain)

    def record(self, clearn, clock):
        @always(clock.posedge)
        def recorder():
            if clearn == clearn.active:
                #self.samples = []
                self.samples_i = []
                self.samples_q = []
            elif self.valid:
                self.samples_i.append(int(self.i))
                self.samples_q.append(int(self.q))
            else:
                self.samples_i.append(np.nan)
                self.samples_q.append(np.nan)
        return recorder

def offset_corrector(clearn, clock,
        correct_i, correct_q,
        in_sign,
        out_sign):

    in_valid = in_sign.valid
    in_last = in_sign.last
    in_i = in_sign.i
    in_q = in_sign.q

    out_valid = out_sign.valid
    out_last = out_sign.last
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def offset_correct():
        if in_valid:
            out_valid.next = in_valid
            out_last.next = in_last
            out_i.next = in_i + correct_i  # TODO: saturate!
            out_q.next = in_q + correct_q
        else:
            out_valid.next = False
            out_last.next = False
            out_i.next = 0
            out_q.next = 0

    return offset_correct

def binary_offseter(clearn, clock,
                    in_sign,
                    out_sign):

    in_valid = in_sign.valid
    in_last = in_sign.last
    in_i = in_sign.i
    in_q = in_sign.q

    out_valid = out_sign.valid
    out_last = out_sign.last
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def binary_offset():
        if in_valid:
            out_valid.next = True
            out_last.next = in_last
            out_i.next = intbv(concat(not in_i[len(in_i) - 1], in_i[len(in_i) - 1:]), min=0, max=2**len(in_i))
            out_q.next = intbv(concat(not in_q[len(in_q) - 1], in_q[len(in_q) - 1:]), min=0, max=2**len(in_q))
        else:
            out_valid.next = False
            out_last.next = False
            out_i.next = 0
            out_q.next = 0

    return binary_offset

def iqmux(clearn, clock,
        channel,
        in0_sign,
        in1_sign,
        out_sign):

    in0_valid = in0_sign.valid
    in0_i = in0_sign.i
    in0_q = in0_sign.q
    in0_last = in0_sign.last

    in1_valid = in1_sign.valid
    in1_i = in1_sign.i
    in1_q = in1_sign.q
    in1_last = in1_sign.last

    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q
    out_last = out_sign.last

    @always_seq(clock.posedge, reset=clearn)
    def mux():
        if channel == 0:
            out_valid.next = in0_valid
            out_last.next = in0_last
            out_i.next = in0_i
            out_q.next = in0_q
        else:
            out_valid.next = in1_valid
            out_last.next = in1_last
            out_i.next = in1_i
            out_q.next = in1_q

    return mux

def iqdemux(clearn, clock,
        channel,
        in_sign,
        out0_sign,
        out1_sign):

    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last

    out0_valid = out0_sign.valid
    out0_i = out0_sign.i
    out0_q = out0_sign.q
    out0_last = out0_sign.last

    out1_valid = out1_sign.valid
    out1_i = out1_sign.i
    out1_q = out1_sign.q
    out1_last = out1_sign.last

    @always_seq(clock.posedge, reset=clearn)
    def demux():
        if channel == 0:
            out0_valid.next = in_valid
            out0_last.next = in_last
            out0_i.next = in_i
            out0_q.next = in_q
            out1_valid.next = False
            out1_last.next = False
            out1_i.next = 0
            out1_q.next = 0
        else:
            out0_valid.next = False
            out0_last.next = False
            out0_i.next = 0
            out0_q.next = 0
            out1_valid.next = in_valid
            out1_last.next = in_last
            out1_i.next = in_i
            out1_q.next = in_q

    return demux

