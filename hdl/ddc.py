import math
from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal
from scipy.special import binom

from dsp import Signature, offset_corrector, binary_offseter
from dsp import iqmux
from dsp import accumulator, comb, truncator

def downsampler(clearn, clock, in_sign, out_sign, decim):
    """Takes every ``decim`` samples.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :param decim: The decimation factor.
    :returns: A synthesizable MyHDL instance.
    """
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
    def downsample():
        if in_valid:
            if cnt == 0:
                out_i.next = in_i
                out_q.next = in_q
                out_last.next = in_last
                out_valid.next = True
                cnt.next = decim - 1
            else:
                out_i.next = 0
                out_q.next = 0
                out_valid.next = False
                out_last.next = False
                cnt.next = cnt - 1
        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False
            out_last.next = False
            cnt.next = 0

    return downsample

def cic_decim_max_bits(in_len, decim, cic_order, cic_delay):
    return int(math.ceil(cic_order * math.log(decim * cic_delay, 2) + in_len) - 1)

def cic_decim_bit_truncation(in_len, out_len, decim, cic_order, cic_delay, stage):
    """Returns how many bits to truncate from a stage in a CIC decmiator."""
    # Use the nomenclature from Hogenauer
    N = cic_order
    M = cic_delay
    R = decim
    Bin = in_len
    Bout = out_len

    # First define the CIC impulse response function
    ha = lambda j, k: sum([( (-1)**l * binom(N, l) * binom(N - j + k - R * M * l, k - R * M * l)) for l in range(0, int(math.floor(k / (R * M)) + 1))])
    hb = lambda j, k: (-1)**k * binom(2 * N + 1 - j, k)
    def h(j, k):
        if j in range(1, N + 1):
            return ha(j, k)
        else:
            return hb(j, k)

    # This is the variance error gain at stage j
    def F(j):
        if j in range(1, 2 * N + 1):
            return math.sqrt(sum([h(j, k) ** 2 for k in range(0, (R * M - 1) * N + j - 1)]))
        else:
            return 1.0
    
    # Find the maximum number of bits that would be truncated
    Bmax = cic_decim_max_bits(in_len, decim, cic_order, cic_delay)
    B2N1 = Bmax - Bout + 1
    # And find it's variance
    sigma2N1 = math.sqrt((1/12.) * ((2 ** B2N1) ** 2))
    sigmaT2N1 = math.sqrt((sigma2N1 ** 2) * (F(2 * N + 1) ** 2))

    # Finally, compute how many bits to discard in this stage to keep the
    # overall error below the final stage's truncation
    return int(math.floor(-math.log(F(stage), 2) + math.log(sigmaT2N1, 2) + .5 * math.log(6./N, 2)))

def cic_decim_gain(decim, cic_order, cic_delay):
    """Maximum gain in a CIC decimator."""
    return (decim * cic_delay) ** cic_order

def cic_decim_register_width(Bin, Bout, R, N, M, j):
    """Get the register width at stage j."""
    Bmax = cic_decim_max_bits(Bin, R, N, M)+3  # TODO: huh?
    #return Bmax - cic_decim_bit_truncation(Bin, Bout, R, N, M, j)
    return Bmax

def cic_decim(clearn, clock,
        in_sign,
        out_sign,
        decim,
        #shift,
        cic_order=4, cic_delay=2, **kwargs):
    """A cic decimating filter with given order and delay.

    :param clearn: Reset the filter.
    :param clock: The clock.
    :param in_sign: The input signature.
    :param out_sign: The output signature.
    :param decim: The decimation of the cic filter.
    :param shift: How much to shift the result.
    :param cic_order: The order of the CIC filter.
    :param cic_delay: The delay of the CIC comb elements.
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
    rates = kwargs.get('rates', [decim])
    max_rate = max(rates)

    accumed = [Signature('accumed_%d' % i, True, bits=cic_decim_register_width(
            len(in_i), len(out_i), max_rate, cic_order, cic_delay,
            i)) for i in range(1, cic_order + 1)]
    print 'accumed', accumed
    combed = [Signature('combed_%d' % i, True, bits=cic_decim_register_width(
            len(in_i), len(out_i), max_rate, cic_order, cic_delay,
            cic_order + i)) for i in range(1, cic_order + 1)]
    print 'combed', combed

    accums = []
    for n in range(cic_order):
        if n == 0:
            i = in_sign
        else:
            i = accumed[n - 1]

        o = accumed[n]

        print 'accum in=%d, out=%d' % (len(i.q), len(o.q))
        accums.append(accumulator(clearn, clock, i, o, n))  # TODO truncate

    decimated = accumed[cic_order - 1].copy('decimated')
    decimator_0 = downsampler(clearn, clock,
        accumed[cic_order - 1], decimated,
        decim)

    combs = []
    for n in range(cic_order):
        if n == 0:
            i = decimated
        else:
            i = combed[n - 1]

        o = combed[n]

        print 'comb in=%d, out=%d' % (len(i.q), len(o.q))
        combs.append(comb(clearn, clock, cic_delay, i, o))  # TODO truncate
    
    # TODO: does this need a shifter?  yeah.
    truncator_0 = truncator(clearn, clock,
        combed[cic_order - 1], out_sign)

    instances = accums, decimator_0, combs, truncator_0

    if kwargs.get('sim', None):
        sim = kwargs['sim']
        sim.record(in_sign)
        [sim.record(a) for a in accumed]
        sim.record(decimated)
        [sim.record(c) for c in combed]
        sim.record(out_sign)

    return instances
