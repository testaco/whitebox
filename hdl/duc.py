from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

from dsp import Signature
from dsp import offset_corrector, binary_offseter
from dsp import iqmux, iqdemux

from dds import dds as DDS
DDS_NUM_SAMPLES=256

def truncator(clearn, in_clock, in_sign, out_sign):
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

    #print 'truncator from', in_sign.bits, 'to', out_sign.bits, 'and so', i_msb, i_lsb, i_msb - i_lsb

    @always_seq(in_clock.posedge, reset=clearn)
    def truncate():
        if in_valid:
            out_valid.next = True
            # TODO: is this right???
            out_i.next = in_i[i_msb:i_lsb].signed()
            out_q.next = in_q[q_msb:q_lsb].signed()
            out_last.next = in_last
        else:
            out_valid.next = False
            out_last.next = False 
            out_i.next = 0
            out_q.next = 0

    return truncate

def upsampler(clearn, clock, in_sign, out_sign, interp):
    cnt = Signal(intbv(0)[len(interp):])
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
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
            out_last.next = in_last
        elif cnt > 0:
            out_valid.next = True
            cnt.next = cnt - 1
        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False
            out_last.next = False

    return upsample
        
def interpolator(clearn, clock, in_sign, out_sign, interp):
    cnt = Signal(intbv(0)[len(interp):])
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def interpolate():
        if in_valid:
            out_i.next = in_i #in_i[16:6].signed()
            out_q.next = in_q #in_q[16:6].signed()
            out_valid.next = True
            out_last.next = in_last
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
            out_last.next = False

    return interpolate

def decimator(clearn, clock, in_sign, out_sign, decim):
    cnt = Signal(intbv(0)[len(decim):])
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def decimate():
        if in_valid:
            cnt.next = cnt + 1

            if cnt == decim - 1:
                cnt.next = 0
                out_i.next = in_i #in_i[16:6].signed()
                out_q.next = in_q #in_q[16:6].signed()
                out_valid.next = True
                out_last.next = in_last
            else:
                out_i.next = 0
                out_q.next = 0
                out_valid.next = False
                out_last.next = False

        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False
            out_last.next = False

    return decimate

def pass_through(clearn, clock, in_sign, out_sign):
    in_valid = in_sign.valid
    in_i = in_sign.i
    in_q = in_sign.q
    in_last = in_sign.last
    out_last = out_sign.last
    out_valid = out_sign.valid
    out_i = out_sign.i
    out_q = out_sign.q

    @always_seq(clock.posedge, reset=clearn)
    def p():
        if in_valid:
            out_valid.next = True
            out_last.next = in_last
            out_i.next = in_i
            out_q.next = in_q
        else:
            out_valid.next = False
            out_last.next = False
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

def accumulator(clearn, clock, in_sign, out_sign):
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
            out_i.next = out_i + in_i
            out_q.next = out_q + in_q
        else:
            out_valid.next = False
            out_last.next = False
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
    in_last = in_sign.last
    out_last = out_sign.last
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

def interleaver(clearn, clock, clock2x,
        in_sign,
        out_valid, out_data, out_last):

    in_valid = in_sign.valid
    in_last = in_sign.last
    in_i = in_sign.i
    in_q = in_sign.q

    phase = Signal(bool(0))
    i = Signal(intbv(0, min=0, max=2**10))
    q = Signal(intbv(0, min=0, max=2**10))
    valid = Signal(bool(0))
    last = Signal(bool(0))

    @always_seq(clock.posedge, reset=clearn)
    def innie():
        if in_valid:
            i.next = in_i
            q.next = in_q
            valid.next = True
            last.next = in_last
        elif not phase:
            valid.next = False
        else:
            i.next = 0
            q.next = 0
            valid.next = False
            last.next = False

    @always_seq(clock2x.posedge, reset=clearn)
    def outie():
        if valid:
            phase.next = not phase
            out_valid.next = True
            out_last.next = last
            out_data.next = i if phase else q
        else:
            out_valid.next = False
            out_last.next = False
            out_data.next = 0

    return innie, outie

def duc_fake(clearn, dac_clock, dac2x_clock,
        fifo_empty, fifo_re, fifo_rdata,
        system_txen, system_txstop,
        system_ddsen, system_filteren,
        system_interp, system_fcw,
        system_correct_i, system_correct_q,
        underrun, sample,
        dac_en, dac_data, dac_last, **kwargs):

    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

    @always_seq(dac_clock.posedge, reset=clearn)
    def pass_through():
        underrun.next = 0
        fifo_re.next = 0

    @always_seq(dac2x_clock.posedge, reset=clearn)
    def pass_through2():
        dac_en.next = 0
        dac_data.next = 0
        dac_last.next = 0

    return pass_through, pass_through2

def duc(clearn, dac_clock, dac2x_clock,
        loopen, loopback,
        fifo_empty, fifo_re, fifo_rdata,
        system_txen, system_txstop,
        system_ddsen, system_filteren,
        system_interp, system_fcw,
        system_correct_i, system_correct_q,
        underrun, sample,
        dac_en, dac_data, dac_last, **kwargs):

    dspsim = kwargs.get('dspsim', None)
    interp_default = kwargs.get('interp', 1)

    chain = []

    sync_txen = Signal(bool(0))
    txen = Signal(bool(0))
    sync_txstop = Signal(bool(0))
    txstop = Signal(bool(0))
    sync_ddsen = Signal(bool(0))
    ddsen = Signal(bool(0))
    sync_filteren = Signal(bool(0))
    filteren = Signal(bool(0))
    sync_interp = Signal(intbv(interp_default)[len(system_interp):])
    interp = Signal(intbv(interp_default)[len(system_interp):])
    sync_fcw = Signal(intbv(0)[len(system_fcw):])
    fcw = Signal(intbv(0)[len(system_fcw):])
    sync_correct_i = Signal(intbv(0)[len(system_correct_i):])
    correct_i = Signal(intbv(0)[len(system_correct_i):])
    sync_correct_q = Signal(intbv(0)[len(system_correct_q):])
    correct_q = Signal(intbv(0)[len(system_correct_q):])
    re = Signal(bool(0))

    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

    #truncated_0 = Signature("truncated0", True, bits=8)
    #truncator_0 = truncator(clearn, dac_clock, sample, truncated_0)

    truncated_1 = Signature("truncated1", True, bits=10)
    truncator_1 = truncator(clearn, dac_clock, sample, truncated_1)
    chain.append(truncator_1)

    upsampled = Signature("upsampled", True, bits=10)
    upsampler_0 = upsampler(clearn, dac_clock, truncated_1, upsampled, interp)
    chain.append(upsampler_0)

    if kwargs.get('cic_enable', True):
        filtered = Signature("filtered", True, bits=10)
        cic_0 = cic(clearn, dac_clock, truncated_0, filtered,
                interp,
                cic_order=3, cic_delay=1,
                sim=dspsim)
        chain.append(cic_0)

        processed = Signature("processed", True, bits=10)
        processed_mux = iqmux(clearn, dac_clock,
                filteren,
                upsampled, filtered, processed)
        chain.append(processed_mux)
    else:
        processed = upsampled

    if kwargs.get('dds_enable', True):
        dds_out = Signature("dds_out", True, bits=10)
        dds_args = clearn, dac_clock, ddsen, dds_out, fcw
        dds = DDS(*dds_args, num_samples=DDS_NUM_SAMPLES)
        chain.append(dds)

        rf_out = Signal(bool(0))
        rf_out_i = Signal(intbv(0, min=-2**9, max=2**9))
        rf_out_q = Signal(intbv(0, min=-2**9, max=2**9))
        rf_out = Signature("rf_out", True, bits=10)
        dds_mux = iqmux(clearn, dac_clock,
                ddsen,
                processed,
                dds_out,
                rf_out)
        chain.append(dds_mux)
    else:
        rf_out = processed

    if kwargs.get('conditioning_enable', True):
        corrected = Signature("corrected", True, bits=10)
        corrector = offset_corrector(clearn, dac_clock,
                correct_i, correct_q,
                rf_out, corrected)
        chain.append(corrector)
        
        offset = Signature("offset", True, bits=10)
        offseter = binary_offseter(clearn, dac_clock,
                corrected, offset)
        chain.append(offseter)
    else:
        offset = rf_out

    conditioned = Signature("conditioned", True, bits=10)
    loopback_0 = iqdemux(clearn, dac_clock, loopen, offset, conditioned, loopback)
    chain.append(loopback_0)

    interleaver_0 = interleaver(clearn, dac_clock, dac2x_clock,
            conditioned, 
            dac_en, dac_data, dac_last)
    chain.append(interleaver_0)

    @always_seq(dac_clock.posedge, reset=clearn)
    def synchronizer():
        sync_txen.next = system_txen
        txen.next = sync_txen
        sync_txstop.next = system_txstop
        txstop.next = sync_txstop
        sync_ddsen.next = system_ddsen
        ddsen.next = sync_ddsen
        sync_filteren.next = system_filteren
        filteren.next = sync_filteren
        sync_interp.next = system_interp
        interp.next = sync_interp
        sync_fcw.next = system_fcw
        fcw.next = sync_fcw
        sync_correct_i.next = system_correct_i
        correct_i.next = sync_correct_i
        sync_correct_q.next = system_correct_q
        correct_q.next = sync_correct_q

    interp_counter = Signal(intbv(0)[32:])

    @always_seq(dac_clock.posedge, reset=clearn)
    def consumer():
        if txen:
            if interp_counter == 0:
                interp_counter.next = interp - 1
                fifo_re.next = True
                re.next = True
            else:
                interp_counter.next = interp_counter - 1

            if re:
                # Watch for underrun
                if fifo_empty and not txstop:
                    underrun.next = underrun + 1
                sample_i.next = fifo_rdata[16:].signed()
                sample_q.next = fifo_rdata[32:16].signed()
                sample_valid.next = True
                sample_last.next = txstop if fifo_empty else False
                fifo_re.next = False
                re.next = False
            else:
                sample_valid.next = False
    
    return synchronizer, consumer, chain
