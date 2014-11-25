import math
import numpy as np
from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

SIGNATURE_SIGNED = True
SIGNATURE_UNSIGNED = False

class Signature(object):
    """Represents a small register file of a handshake for the DSP signal flow.

    :param name: Human readable
    :param bits: Bit precision of each channel
    :param min: The minimum value allowed
    :param max: The maximum value allowed
    :param valid: An already existing boolean ``Signal``
    :param last: An already existing boolean ``Signal``
    :param i: An already existing intbv ``Signal``
    :param q: An already existing intbv ``Signal``
    """
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
        """Get the signature as an ``intbv``."""
        return intbv(int(default), min=self.min, max=self.max)

    def myhdl_overflow_acceptable(self, default):
        """Get the signature as a ``modbv``."""
        return modbv(int(default), min=self.min, max=self.max)

    def numpy_dtype(self):
        """Get the signature as a numpy dtype."""
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
        """A myhdl stimulus what will record the valid dsp stream and save
        it to ``self.samples_i`` and ``self.samples_q``.

        :param clearn: A myhdl ``Signal``.
        :param clock: A myhdl ``Signal``.
        :returns: A simulation capable MyHDL object.
        """
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
    """Analog quadrature offset corrector.

    Lets you add a DC offset to the incoming signal.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param correct_i: An intbv to add to each sample's i channel.
    :param correct_q: An intbv to add to each sample's q channel.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :returns: A synthesizable MyHDL instance.
    """
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

def offset_corrector_with_saturate(clearn, clock,
        correct_i, correct_q,
        in_sign,
        out_sign, saturated):

    in_valid = in_sign.valid
    in_last = in_sign.last
    in_i = in_sign.i
    in_q = in_sign.q

    internal_valid = Signal(bool(0))
    internal_last = Signal(bool(0))
    internal_i = Signal(intbv(0, min=-2**(len(in_i)+1), max=2**(len(in_i)+1)))
    internal_q = Signal(intbv(0, min=-2**(len(in_q)+1), max=2**(len(in_q)+1)))

    out_valid = out_sign.valid
    out_last = out_sign.last
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def offset_correct():
        if in_valid:
            internal_valid.next = in_valid
            internal_last.next = in_last
            internal_i.next = in_i + correct_i
            internal_q.next = in_q + correct_q
        else:
            internal_valid.next = False
            internal_last.next = False
            internal_i.next = 0
            internal_q.next = 0

        if internal_valid:
            out_valid.next = internal_valid
            out_last.next = internal_last
            if internal_i[len(in_i)+1:len(in_i)]:
                out_i.next = out_i.max
            else:
                out_i.next = internal_i[len(in_i):].signed()
            if internal_q[len(in_q)+1:len(in_q)]:
                out_q.next = out_q.max
            else:
                out_q.next = internal_q[len(in_q):].signed()
        else:
            out_valid.next = False
            out_last.next = False
            out_i.next = 0
            out_q.next = 0

    return offset_correct

def gain_corrector(clearn, clock,
        gain_i, gain_q,
        in_sign,
        out_sign):
    """Analog quadrature gain corrector.

    Lets you correct for gain imbalance with an AQM.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param correct_i: An intbv to add to multiply each sample's i channel by.
    :param correct_q: An intbv to add to multiply each sample's q channel by.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :returns: A synthesizable MyHDL instance.
    """

    in_valid = in_sign.valid
    in_last = in_sign.last
    in_i = in_sign.i
    in_q = in_sign.q

    out_valid = out_sign.valid
    out_last = out_sign.last
    out_i = out_sign.i
    out_q = out_sign.q

    s = len(in_i) + len(gain_i)
    mul_i = Signal(intbv(0, min=-2**s, max=2**s))
    mul_q = Signal(intbv(0, min=-2**s, max=2**s))
    mul_valid = Signal(bool(False))
    mul_last = Signal(bool(False))

    @always_seq(clock.posedge, reset=clearn)
    def gain_correct():
        if in_valid:
            mul_valid.next = in_valid
            mul_last.next = in_last

            #print 'i', ia.signed()/2.**9, '*', ix.signed() / float(ix.max), '=', ia.signed()/2**9 * (ix.signed() / float(ix.max))

            mul_i.next = in_i.signed() * concat(bool(0), gain_i).signed()
            mul_q.next = in_q.signed() * concat(bool(0), gain_q).signed()
        else:
            mul_valid.next = False
            mul_last.next = False
            mul_i.next = 0
            mul_q.next = 0

        if mul_valid:
            out_valid.next = mul_valid
            out_last.next = mul_last
            #print 'm', mul_q[len(mul_q)-2] ^ mul_q[len(mul_q)-3]
            out_i.next = mul_i[len(mul_i)-2:len(mul_i)-len(out_i)-2].signed()
            out_q.next = mul_q[len(mul_q)-2:len(mul_q)-len(out_q)-2].signed()
        else:
            out_valid.next = False
            out_last.next = False
            out_i.next = 0
            out_q.next = 0

    return gain_correct

def binary_offseter(clearn, clock,
                    in_sign,
                    out_sign):
    """Converts from two's complement to binary offset.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :returns: A synthesizable MyHDL instance.
    """

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
    """Multiplex between two incoming signals.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param channel: The selected channel.
    :param in1_sign: The first incomming signature.
    :param in2_sign: The second incomming signature.
    :param out_sign: The outgoing signature.
    :returns: A synthesizable MyHDL instance.
    """

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
    """Decoder two outgoing ports.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param channel: The selected channel.
    :param in_sign: The incomming signature.
    :param out1_sign: The first outgoing signature.
    :param out2_sign: The second outgoing signature.
    :returns: A synthesizable MyHDL instance.
    """

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

def accumulator(clearn, clock, in_sign, out_sign, n=0):
    """A simple accumulator.

    :param clearn: The reset the accumulator.
    :param clock: The clock.
    :param in_sign: The input signature.
    :param out_sign: The output signature.
    :returns: A synthesizable MyHDL instance.
    """
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def accumulate():
        if in_valid:
            out_valid.next = True
            out_last.next = in_last
            #print 'accum%d in=%d out=%d next=%d' % (n, in_i, out_i, in_i + out_i)
            out_i.next = out_i + in_i
            out_q.next = out_q + in_q
        else:
            out_valid.next = False
            out_last.next = False
            out_i.next = 0
            out_q.next = 0

    return accumulate

def delay_1(clearn, clock, sign, x, y):
    valid = sign.valid
    @always_seq(clock.posedge, reset=clearn)
    def delay():
        if valid:
            y.next = x
    return delay

def delay_2(clearn, clock, sign, x, y):
    element = Signal(sign.myhdl(0))

    @always_seq(clock.posedge, reset=clearn)
    def delay():
        if sign.valid:
            element.next = x
            y.next = element
    return delay

def delay_n(n, clearn, clock, sign, x, y):
    """Delay a signal ``n`` cycles.

    :param n: A constant, how many cycles to delay.
    :param clearn: The reset signal.
    :param clock: The clock.
    :param sign: The signature.
    :param x: Input.
    :param y: Delayed outpt.
    :returns: A synthesizable MyHDL instance.
    """
    if n == 1:
        return delay_1(clearn, clock, sign, x, y)
    elif n == 2:
        return delay_2(clearn, clock, sign, x, y)

    elements = [Signal(sign.myhdl(0)) for i in range(n - 1)]
    
    @always_seq(clock.posedge, reset=clearn)
    def delay():
        if sign.valid:
            elements[0].next = x
            for i in range(1, n - 1):
                elements[i].next = elements[i - 1]
            y.next = elements[n - 2]

    return delay

def comb(clearn, clock, delay, in_sign, out_sign):
    """A comber with ``delay``.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param delay: A constant, how many cycles to delay.
    :param in_sign: The input signature.
    :param out_sign: The output signature.
    :returns: A synthesizable MyHDL instance.
    """
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    delayed_i = Signal(in_sign.myhdl(0))
    delayed_q = Signal(in_sign.myhdl(0))

    delay_i = delay_n(delay, clearn, clock, in_sign, in_i, delayed_i)
    delay_q = delay_n(delay, clearn, clock, in_sign, in_q, delayed_q)

    @always_seq(clock.posedge, reset=clearn)
    def comber():
        if in_valid:
            out_valid.next = True
            out_last.next = in_last
            out_i.next = in_i - delayed_i
            out_q.next = in_q - delayed_q
        else:
            out_valid.next = False
            out_last.next = False
            out_i.next = 0
            out_q.next = 0

    return comber, delay_i, delay_q

def truncator(clearn, in_clock, in_sign, out_sign, **kwargs):
    """Truncates ``in_sign`` to ``out_sign``.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :returns: A synthesizable MyHDL instance.
    """
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    i_msb = len(in_i)
    i_lsb = len(in_i) - len(out_i)
    q_msb = len(in_q)
    q_lsb = len(in_q) - len(out_q)

    assert i_lsb >= 0 and q_lsb >= 0

    debug = kwargs.get('debug', False)
    if debug:
        print 'truncator from', in_sign.bits, 'bits to', out_sign.bits, 'bits; so [%d:%d]' % (i_msb, i_lsb), 'which is', i_msb - i_lsb, 'bits long'

    @always_seq(in_clock.posedge, reset=clearn)
    def truncate():
        if in_valid:
            out_valid.next = True
            out_i.next = in_i[i_msb:i_lsb].signed()
            out_q.next = in_q[q_msb:q_lsb].signed()
            out_last.next = in_last
            if debug:
                print in_i, out_i
        else:
            out_valid.next = False
            out_last.next = False 
            out_i.next = 0
            out_q.next = 0

    return truncate
