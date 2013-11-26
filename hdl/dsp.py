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

