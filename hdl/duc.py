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

        self.valid = Signal(bool(0))
        self.i = Signal(intbv(0, min=self.min, max=self.max))
        self.q = Signal(intbv(0, min=self.min, max=self.max))

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

def truncator(clearn, in_clock, in_sign, out_sign):
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    i_msb = len(in_i)
    i_lsb = len(in_i) - len(out_i)
    q_msb = len(in_q)
    q_lsb = len(in_q) - len(out_q)

    assert i_lsb >= 0 and q_lsb >= 0

    #print 'truncator from', in_sign.bits, 'to', out_sign.bits, 'and so', i_msb, i_lsb, i_msb - i_lsb


    @always_seq(in_clock.posedge, reset=clearn)
    def truncate():
        if in_valid:
            out_valid.next = True
            # TODO: this adds DC bias
            out_i.next = in_i[i_msb:i_lsb].signed()
            out_q.next = in_q[q_msb:q_lsb].signed()
        else:
            out_valid.next = False
            out_i.next = 0
            out_q.next = 0

    return truncate

def upsampler(clearn, clock, in_sign, out_sign, interp):
    cnt = Signal(intbv(0)[len(interp):])
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def upsample():
        if in_valid:
            out_i.next = in_i
            out_q.next = in_q
            out_valid.next = True
            cnt.next = interp - 1
        elif cnt > 0:
            out_valid.next = True
            cnt.next = cnt - 1
        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False

    return upsample
        
def interpolator(clearn, clock, in_sign, out_sign, interp):
    cnt = Signal(intbv(0)[len(interp):])
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def interpolate():
        if in_valid:
            out_i.next = in_i #in_i[16:6].signed()
            out_q.next = in_q #in_q[16:6].signed()
            out_valid.next = True
            cnt.next = interp - 1
        elif cnt > 0:
            # Turn off these two lines for the old version of the interpolator
            out_i.next = 0
            out_q.next = 0
            out_valid.next = True
            cnt.next = cnt - 1
        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False

    return interpolate

def decimator(clearn, clock, in_sign, out_sign, decim):
    cnt = Signal(intbv(0)[len(decim):])
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def decimate():
        if in_valid:
            cnt.next = cnt + 1

            if cnt == decim -1:
                cnt.next = 0
                out_i.next = in_i #in_i[16:6].signed()
                out_q.next = in_q #in_q[16:6].signed()
                out_valid.next = True
            else:
                out_i.next = 0
                out_q.next = 0
                out_valid.next = False

        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False

    return decimate

def pass_through(clearn, clock, in_sign, out_sign):
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def p():
        if in_valid:
            out_valid.next = True
            out_i.next = in_i
            out_q.next = in_q
        else:
            out_valid.next = False
            out_i.next = 0
            out_q.next = 0

    return p

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
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
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
            out_i.next = in_i - delayed_i
            out_q.next = in_q - delayed_q
        else:
            out_valid.next = False
            out_i.next = 0
            out_q.next = 0

    return comber, delay_i, delay_q

def accumulator(clearn, clock, in_sign, out_sign):
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q
    @always_seq(clock.posedge, reset=clearn)
    def accumulate():
        if in_valid:
            out_valid.next = True
            out_i.next = out_i + in_i
            out_q.next = out_q + in_q
        else:
            out_valid.next = False
            out_i.next = 0
            out_q.next = 0

    return accumulate

def cic_total_gain(interp, cic_delay, cic_order):
    combed_gain = lambda i: (cic_delay * interp) ** (i)
    return combed_gain(cic_order)/interp

def cic_bit_gain(interp, cic_delay, cic_order):
    return math.ceil(
            math.log(((cic_delay * interp) ** cic_order)/interp, 2))

def cic(clearn, clock,
        in_sign,
        out_sign,
        interp,
        cic_order=3, cic_delay=5, **kwargs):
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    #print 'CIC order=%d delay=%d interp=%d' % (cic_order, cic_delay, interp)

    combed_gain = lambda i: ((cic_delay * interp) ** (i+1))/interp
    accum_gain = lambda i: combed_gain(cic_order-1)

    combed = [in_sign.copy_with_gain('combed_%d' % i, combed_gain(i)) for i in range(cic_order)]
    accumed = [in_sign.copy_with_gain('accumed_%d' % i, accum_gain(i)) for i in range(cic_order)]

    combs = []
    for n in range(cic_order):
        if n == 0:
            i = in_sign # interpolated
        else:
            i = combed[n - 1]

        o = combed[n]

        #print 'comb stage', n, i, '->', o

        combs.append(comb(clearn, clock, cic_delay, i, o))

    interpolated = combed[cic_order - 1].copy('interpolated')
    interpolator_0 = interpolator(clearn, clock,
        combed[cic_order - 1], interpolated,
        interp)

    #print 'interpolator', combed[cic_order - 1], '->', interpolated

    accums = []
    for n in range(cic_order):
        if n == 0:
            i = interpolated #combed[cic_order - 1]
        else:
            i = accumed[n - 1]

        o = accumed[n]

        #print 'accum stage', n, i, '->', o

        accums.append(accumulator(clearn, clock, i, o))

    #print 'decimated', accumed[cic_order - 1], '->', out_sign

    truncator_2 = truncator(clearn, clock,
        accumed[cic_order - 1], out_sign)
    
    instances = combs, interpolator_0, accums, truncator_2

    if kwargs.get('sim', None):
        sim = kwargs['sim']
        sim.record(in_sign)
        [sim.record(c) for c in combed]
        sim.record(interpolated)
        [sim.record(a) for a in accumed]
        sim.record(out_sign)

    return instances

def iqmux(clearn, clock,
        channel,
        in0_valid, in0_i, in0_q,
        in1_valid, in1_i, in1_q,
        out_valid, out_i, out_q):

    @always_seq(clock.posedge, reset=clearn)
    def mux():
        if channel == 0:
            out_valid.next = in0_valid
            out_i.next = in0_i
            out_q.next = in0_q
        else:
            out_valid.next = in1_valid
            out_i.next = in1_i
            out_q.next = in1_q

    return mux

def offset_corrector(clearn, clock,
        correct_i, correct_q,
        in_valid, in_i, in_q,
        out_valid, out_i, out_q):

    @always_seq(clock.posedge, reset=clearn)
    def offset_correct():
        if in_valid:
            out_valid.next = in_valid
            out_i.next = in_i + correct_i  # TODO: saturate!
            out_q.next = in_q + correct_q
        else:
            out_valid.next = False
            out_i.next = 0
            out_q.next = 0

    return offset_correct

def binary_offseter(clearn, clock,
        in_valid, in_i, in_q,
        out_valid, out_i, out_q):

    @always_seq(clock.posedge, reset=clearn)
    def binary_offset():
        if in_valid:
            out_valid.next = True
            out_i.next = intbv(concat(not in_i[len(in_i) - 1], in_i[len(in_i) - 1:]), min=0, max=2**len(in_i))
            out_q.next = intbv(concat(not in_q[len(in_q) - 1], in_q[len(in_q) - 1:]), min=0, max=2**len(in_q))
        else:
            out_valid.next = False
            out_i.next = 0
            out_q.next = 0

    return binary_offset

def interleaver(clearn, clock, clock2x,
        in_valid, in_i, in_q,
        out_valid, out_data):

    phase = Signal(bool(0))
    i = Signal(intbv(0, min=0, max=2**10))
    q = Signal(intbv(0, min=0, max=2**10))
    valid = Signal(bool(0))

    @always_seq(clock.posedge, reset=clearn)
    def innie():
        if in_valid:
            i.next = in_i
            q.next = in_q
            valid.next = True
        elif not phase:
            valid.next = False
        else:
            i.next = 0
            q.next = 0
            valid.next = False

    @always_seq(clock2x.posedge, reset=clearn)
    def outie():
        if valid:
            phase.next = not phase
            out_valid.next = True
            out_data.next = i if phase else q
        else:
            out_valid.next = False
            out_data.next = 0

    return innie, outie

