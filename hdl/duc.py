import math

from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

from dsp import Signature
from dsp import offset_corrector, binary_offseter, gain_corrector
from dsp import iqmux, iqdemux
from dsp import accumulator, comb, truncator
from fir import fir as FIR
from ddc import downsampler, cic_decim

from dds import dds as DDS
DDS_NUM_SAMPLES=256

def shifting_truncator(clearn, in_clock, in_sign, out_sign, shift, **kwargs):
    """Truncates ``in_sign`` to ``out_sign``, by shifting right ``shift``
    number of bits.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :param shift: The ammount to shift by.
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

    @always_seq(in_clock.posedge, reset=clearn)
    def shift_and_truncate():
        if in_valid:
            out_valid.next = True
            if shift == 0:
                out_i.next = in_i[len(out_i)+0:0].signed()
                out_q.next = in_q[len(out_q)+0:0].signed()
            elif shift == 1:
                out_i.next = in_i[len(out_i)+1:1].signed()
                out_q.next = in_q[len(out_q)+1:1].signed()
            elif shift == 3:
                out_i.next = in_i[len(out_i)+3:3].signed()
                out_q.next = in_q[len(out_q)+3:3].signed()
            elif shift == 4:
                out_i.next = in_i[len(out_i)+4:4].signed()
                out_q.next = in_q[len(out_q)+4:4].signed()
            elif shift == 5:
                out_i.next = in_i[len(out_i)+5:5].signed()
                out_q.next = in_q[len(out_q)+5:5].signed()
            elif shift == 6:
                out_i.next = in_i[len(out_i)+6:6].signed()
                out_q.next = in_q[len(out_q)+6:6].signed()
            elif shift == 7:
                out_i.next = in_i[len(out_i)+7:7].signed()
                out_q.next = in_q[len(out_q)+7:7].signed()
            elif shift == 8:
                out_i.next = in_i[len(out_i)+8:8].signed()
                out_q.next = in_q[len(out_q)+8:8].signed()
            elif shift == 9:
                out_i.next = in_i[len(out_i)+9:9].signed()
                out_q.next = in_q[len(out_q)+9:9].signed()
            elif shift == 10:
                out_i.next = in_i[len(out_i)+10:10].signed()
                out_q.next = in_q[len(out_q)+10:10].signed()
            elif shift == 11:
                out_i.next = in_i[len(out_i)+11:11].signed()
                out_q.next = in_q[len(out_q)+11:11].signed()
            elif shift == 12:
                out_i.next = in_i[len(out_i)+12:12].signed()
                out_q.next = in_q[len(out_q)+12:12].signed()
            elif shift == 13:
                out_i.next = in_i[len(out_i)+13:13].signed()
                out_q.next = in_q[len(out_q)+13:13].signed()
            elif shift == 14:
                out_i.next = in_i[len(out_i)+14:14].signed()
                out_q.next = in_q[len(out_q)+14:14].signed()
            elif shift == 15:
                out_i.next = in_i[len(out_i)+15:15].signed()
                out_q.next = in_q[len(out_q)+15:15].signed()
            elif shift == 16:
                out_i.next = in_i[len(out_i)+16:16].signed()
                out_q.next = in_q[len(out_q)+16:16].signed()
            elif shift == 17:
                out_i.next = in_i[len(out_i)+17:17].signed()
                out_q.next = in_q[len(out_q)+17:17].signed()
            elif shift == 18:
                out_i.next = in_i[len(out_i)+18:18].signed()
                out_q.next = in_q[len(out_q)+18:18].signed()
            elif shift == 19:
                out_i.next = in_i[len(out_i)+19:19].signed()
                out_q.next = in_q[len(out_q)+19:19].signed()
            elif shift == 20:
                out_i.next = in_i[len(out_i)+20:20].signed()
                out_q.next = in_q[len(out_q)+20:20].signed()
            out_last.next = in_last
        else:
            out_valid.next = False
            out_last.next = False 
            out_i.next = 0
            out_q.next = 0

    return shift_and_truncate

def upsampler(clearn, clock, in_sign, out_sign, interp):
    """Upsamples the signal by repeating each one ``interp - 1`` times.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :param interp: How many times to inerpolate.
    :returns: A synthesizable MyHDL instance.
    """
    cnt = Signal(intbv(0)[len(interp):])
    last = Signal(bool(0))
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
        if interp == 1:
            out_i.next = in_i
            out_q.next = in_q
            out_valid.next = in_valid
            out_last.next = in_last
        else:
            if cnt > 0:
                out_valid.next = True
                cnt.next = cnt - 1
                if cnt == 1:
                    out_last.next = last
                else:
                    out_last.next = False
            elif in_valid:
                cnt.next = interp - 1
                out_i.next = in_i
                out_q.next = in_q
                out_valid.next = True
                out_last.next = False
                last.next = in_last
            else:
                out_i.next = 0
                out_q.next = 0
                out_valid.next = False
                out_last.next = False
                last.next = False

    return upsample
        
def interpolator(clearn, clock, in_sign, out_sign, interp):
    """Interpolates the signal by stuffing ``interp - 1`` zeroes between samples.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param in_sign: The incomming signature.
    :param out_sign: The outgoing signature.
    :param interp: How many times to inerpolate.
    :returns: A synthesizable MyHDL instance.
    """
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

def pass_through_with_enable(clearn, clock, in_sign, out_sign, enable):
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
        if enable and in_valid:
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

def cic_interp_coefficients(interp, cic_order, cic_delay):
    """Returns the cic interpolator impulse response coefficients."""
    from scipy.special import binom
    # Use the nomenclature from Hogenauer
    N = cic_order
    M = cic_delay
    R = interp

    # First define the CIC impulse response function
    ha = lambda j, k: sum([( (-1)**l * binom(N, l) *
        binom(N - j + k - R * M * l, k - R * M * l)) \
        for l in range(0, int(math.floor(k / (R * M)) + 1))])
    hb = lambda j, k: (-1)**k * binom(2 * N + 1 - j, k)
    def h(j, k):
        if j in range(1, N + 1):
            return ha(j, k)
        else:
            return hb(j, k)

    # This is the impulse response at the final stage.
    j = 2*N
    return [h(j, k) for k in range(0, (R * M - 1) * N + j - 1)]

def cic_filter_response(interp, cic_order, cic_delay):
    import numpy as np
    import matplotlib.pyplot as plt
    f = np.linspace(1e-5, 3.5, 2048)
    # Hogenauer, eqn. 5
    h_mag = (np.sin(np.pi * cic_delay * f) / np.sin(np.pi * f / interp))**(2 * cic_order)
    # dB w.r.t. DC, since it's a lowpass filter
    h_db = -5 * np.log(h_mag / h_mag[0])
    plt.plot(f, h_db)
    fcutoff = f[np.argmax(h_db >= 3)]
    plt.vlines(fcutoff, 0, 120, color='r', linestyles='dashed', label='fc')
    plt.ylim(120, 0)
    plt.title('CIC Filter Frequency Response (interp=%d, order=%d, delay=%d)' % (interp, cic_order, cic_delay))
    plt.xlabel('Frequency relative to low sampling rate')
    plt.ylabel('Attenuation (dB)')
    plt.show()
    return f, h_db

def cic_gain(interp, cic_order, cic_delay, stage):
    if stage in range(1, cic_order + 1):
        return 2 ** stage
    else:
        return (2 ** (2 * cic_order - stage) * (interp * cic_delay) ** (stage - cic_order)) / interp

def cic_bit_width(in_len, interp, cic_order, cic_delay, stage):
    if cic_delay == 1 and stage == cic_order:
        return int(in_len + cic_order - 1)
    else:
        return int(in_len + math.ceil(math.log(
            cic_gain(interp, cic_order, cic_delay, stage), 2)))

def cic_shift(in_len, out_len, interp, cic_order, cic_delay):
    return max(cic_bit_width(in_len, interp, cic_order, cic_delay, 2 * cic_order) - out_len, 0)

def cic(clearn, clock,
        in_sign,
        out_sign,
        interp,
        shift,
        cic_order=4, cic_delay=2, **kwargs):
    """A cic filter with given order, delay, and interpolation.

    :param clearn: The reset the accumulator.
    :param clock: The clock.
    :param in_sign: The input signature.
    :param out_sign: The output signature.
    :param interp: The interpolation of the cic filter.
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
    rates = kwargs.get('rates', [interp])
    max_rate = max(rates)

    #print 'CIC order=%d delay=%d interp=%d' % (cic_order, cic_delay, interp)

    combed = [Signature('combed_%d' % i, True, bits=cic_bit_width(
            len(in_i), max_rate, cic_order, cic_delay,
            i)) for i in range(1, cic_order + 1)]
    accumed = [Signature('accumed_%d' % i, True, bits=cic_bit_width(
            len(in_i), max_rate, cic_order, cic_delay,
            cic_order + i)) for i in range(1, cic_order + 1)]

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

    #truncator_2 = truncator(clearn, clock,
    #    accumed[cic_order - 1], out_sign, debug=True)
    shifter_0 = shifting_truncator(clearn, clock,
          accumed[cic_order - 1], out_sign,
          shift)
    
    instances = combs, interpolator_0, accums, shifter_0 #truncator_2

    # NOTE: This only works when used with MyHDL's Simulation
    # feature, not Cosimulation with an external Verilog tool.
    # So, for the Whitebox codebase, this means DSP tests can use
    # it but not Cosimulations of the Whitebox toplevel HDL.
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
    """Interleaves the input signature to a interleaved single channel signal.

    :param clearn: The reset signal.
    :param clock: The clock.
    :param clock2x: The double clock rate.
    :param in_sign: The incomming signature.
    :param out_valid: The outgoing valid flag.
    :param out_data: The output data.
    :param out_last: The outgoing last flag.
    :returns: A synthesizable MyHDL instance.
    """

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
    def producer():
        if in_valid:
            i.next = in_i
            q.next = in_q
            valid.next = True
            last.next = in_last
        else:
            i.next = 0
            q.next = 0
            valid.next = False
            last.next = False

    @always_seq(clock2x.posedge, reset=clearn)
    def consumer():
        if valid:
            out_valid.next = True
            out_last.next = last
            out_data.next = q if clock else i
        else:
            out_valid.next = False
            out_last.next = False
            out_data.next = 0

    return producer, consumer

def duc(clearn, dac_clock, dac2x_clock,
        loopen, loopback,
        fifo_empty, fifo_re, fifo_dvld, fifo_rdata, fifo_underflow,
        system_txen, system_txstop,
        system_ddsen, system_filteren,
        system_interp, system_shift,
        system_fcw,
        system_correct_i, system_correct_q,
        system_gain_i, system_gain_q,
        underrun, sample,
        dac_en, dac_data, dac_last,

        rx_fifo_full, rx_fifo_we, rx_fifo_wdata,
        rxen, rxstop, rxfilteren,
        decim, system_rx_correct_i, system_rx_correct_q,
        rx_overrun, rx_sample,
        adc_idata, adc_qdata, adc_last,

        fir_coeff_ram_addr,
        fir_coeff_ram_din0,
        fir_coeff_ram_din1,
        fir_coeff_ram_blk,
        fir_coeff_ram_wen,
        fir_coeff_ram_dout0,
        fir_coeff_ram_dout1,
        fir_delay_line_i_ram_addr,
        fir_delay_line_i_ram_din,
        fir_delay_line_i_ram_blk,
        fir_delay_line_i_ram_wen,
        fir_delay_line_i_ram_dout,
        fir_delay_line_q_ram_addr,
        fir_delay_line_q_ram_din,
        fir_delay_line_q_ram_blk,
        fir_delay_line_q_ram_wen,
        fir_delay_line_q_ram_dout,
        system_firen, system_fir_bank1, system_fir_bank0, system_fir_N,
        **kwargs):
    """The Digital Up Converter.    

    :param clearn: Reset signal, completely resets the dsp chain.
    :param dac_clock: The sampling clock.
    :param dac2x_clock: Twice the sampling clock.
    :param loopen: Enable the loopback device.
    :param loopback: The loopback signature to the digital down converter.
    :param fifo_empty: Input signal that the fifo is empty.
    :param fifo_re: Output signal to enable a sample read.
    :param fifo_dvld: Input signal that FIFO data is valid.
    :param fifo_rdata: Input sample data.
    :param fifo_underflow: Input signal that fifo underflowed.
    :param system_txen: Enable transmit.
    :param system_txstop: Stop the transmitter.
    :param system_ddsen: Enable the DDS.
    :param system_filteren: Enable the CIC filter.
    :param system_interp: Interpolation rate.
    :param system_fcw: Set the frequency control word of the DDS.
    :param system_correct_i: Set the i-Channel AQM DC Offset Correction.
    :param system_correct_q: Set the q-Channel AQM DC Offset Correction.
    :param system_gain_i: Set the i-channel AQM gain correction.
    :param system_gain_q: Set the q-channel AQM gain correction.
    :param underrun: Output of number of underruns to RFE.
    :param sample: The sample.
    :param dac_en: Enable DAC on valid data signal.
    :param dac_data: The interleaved DAC data.
    :param dac_last: The last sample going out on the DAC, stops the transmit.
    :returns: A MyHDL synthesizable module.
    """

    dspsim = kwargs.get('dspsim', None)

    # DIGIAL UP CONVERTER
    interp_default = kwargs.get('interp', 1)

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
    sync_shift = Signal(intbv(0)[len(system_shift):])
    shift = Signal(intbv(0)[len(system_shift):])
    sync_firen = Signal(bool(0))
    firen = Signal(bool(0))
    sync_fir_bank0 = Signal(bool(0))
    fir_bank0 = Signal(bool(0))
    sync_fir_bank1 = Signal(bool(0))
    fir_bank1 = Signal(bool(0))
    sync_fir_N = Signal(intbv(0, min=system_fir_N.min, max=system_fir_N.max))
    fir_N = Signal(intbv(0, min=system_fir_N.min, max=system_fir_N.max))
    sync_fcw = Signal(intbv(0)[len(system_fcw):])
    fcw = Signal(intbv(0)[len(system_fcw):])
    sync_correct_i = Signal(intbv(0)[len(system_correct_i):])
    correct_i = Signal(intbv(0)[len(system_correct_i):])
    sync_correct_q = Signal(intbv(0)[len(system_correct_q):])
    correct_q = Signal(intbv(0)[len(system_correct_q):])
    sync_gain_i = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    gain_i = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    sync_gain_q = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    gain_q = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    sync_rx_correct_i = Signal(intbv(0)[len(system_rx_correct_i):])
    rx_correct_i = Signal(intbv(0)[len(system_rx_correct_i):])
    sync_rx_correct_q = Signal(intbv(0)[len(system_rx_correct_q):])
    rx_correct_q = Signal(intbv(0)[len(system_rx_correct_q):])

    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

    rx_sample_valid = rx_sample.valid
    rx_sample_last = rx_sample.last
    rx_sample_i = rx_sample.i
    rx_sample_q = rx_sample.q

    adc_sample = Signature("adc", True, bits=10,
        valid=rxen,
        last=adc_last,
        i=adc_idata,
        q=adc_qdata)

    truncated_1 = Signature("truncated1", True, bits=9)
    truncator_1 = truncator(clearn, dac_clock, sample, truncated_1)
    duc_chain = (truncator_1, )

    if kwargs.get('dds_enable', True):
        if_out = Signature("if_out", True, bits=9)
        dds_args = clearn, dac_clock, ddsen, truncated_1, if_out, fcw
        dds = DDS(*dds_args, num_samples=DDS_NUM_SAMPLES)
        duc_chain = duc_chain + (dds, )
    else:
        if_out = truncated_1

    if kwargs.get('fir_enable', True):
        filtered = Signature("filtered", True, bits=9)
        fir_0 = FIR(clearn, dac_clock, if_out, filtered,
                    fir_coeff_ram_addr,
                    fir_coeff_ram_din0,
                    fir_coeff_ram_din1,
                    fir_coeff_ram_blk,
                    fir_coeff_ram_wen,
                    fir_coeff_ram_dout0,
                    fir_coeff_ram_dout1,
                    fir_delay_line_i_ram_addr,
                    fir_delay_line_i_ram_din,
                    fir_delay_line_i_ram_blk,
                    fir_delay_line_i_ram_wen,
                    fir_delay_line_i_ram_dout,
                    fir_delay_line_q_ram_addr,
                    fir_delay_line_q_ram_din,
                    fir_delay_line_q_ram_blk,
                    fir_delay_line_q_ram_wen,
                    fir_delay_line_q_ram_dout,
                    firen, fir_bank1, fir_bank0, fir_N)
        duc_chain = duc_chain + (fir_0, )
    else:
        filtered = if_out

    upsampled = Signature("upsampled", True, bits=9)
    upsampler_0 = upsampler(clearn, dac_clock, filtered, upsampled, interp)
    duc_chain = duc_chain + (upsampler_0, )

    if kwargs.get('cic_enable', True):
        rate_changed = Signature("rate_changed", True, bits=10)
        cic_0 = cic(clearn, dac_clock, filtered, rate_changed,
                interp,
                shift,
                cic_order=kwargs.get('cic_order', 4),
                cic_delay=kwargs.get('cic_delay', 1),
                sim=dspsim)
        duc_chain = duc_chain + (cic_0, )

        processed = Signature("processed", True, bits=10)
        processed_mux = iqmux(clearn, dac_clock,
                filteren,
                upsampled, rate_changed, processed)
        duc_chain = duc_chain + (processed_mux, )
    else:
        processed = upsampled

    rf_out = processed

    tx_loopback = pass_through_with_enable(clearn, dac_clock,
        rf_out, loopback, loopen)
    duc_chain = duc_chain + (tx_loopback, )

    if kwargs.get('conditioning_enable', True):
        gain_corrected = Signature("gain_corrected", True, bits=10)
        gain_corrector_0 = gain_corrector(clearn, dac_clock,
                gain_i, gain_q,
                rf_out, gain_corrected)
        duc_chain = duc_chain + (gain_corrector_0, )

        corrected = Signature("offset_corrected", True, bits=10)
        offset_corrector_0 = offset_corrector(clearn, dac_clock,
                correct_i, correct_q,
                gain_corrected, corrected)
        duc_chain = duc_chain + (offset_corrector_0, )
        
    else:
        corrected = rf_out

    offset = Signature("binary_offset", False, bits=10)
    offseter = binary_offseter(clearn, dac_clock,
            corrected, offset)
    duc_chain = duc_chain + (offseter, )

    interleaver_0 = interleaver(clearn, dac_clock, dac2x_clock,
            offset, 
            dac_en, dac_data, dac_last)
    duc_chain = duc_chain + (interleaver_0, )

    # DIGITAL DOWN CONVERTER
    rx_offset_corrected = Signature("rx_offset_corrected", True, bits=10)
    rx_offset_corrector = offset_corrector(clearn, dac_clock,
                rx_correct_i, rx_correct_q,
                adc_sample, rx_offset_corrected)
    ddc_chain = (rx_offset_corrector, )

    downsampled = Signature("downsampled", True, bits=10)
    downsampled_i = downsampled.i
    downsampled_q = downsampled.q
    downsampled_valid = downsampled.valid
    downsampler_0 = downsampler(clearn, dac_clock, rx_offset_corrected,
            downsampled, decim)
    ddc_chain = ddc_chain + (downsampler_0, )

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
        sync_shift.next = system_shift
        shift.next = sync_shift
        sync_firen.next = system_firen
        firen.next = sync_firen
        sync_fir_bank1.next = system_fir_bank1
        fir_bank1.next = sync_fir_bank1
        sync_fir_bank0.next = system_fir_bank0
        fir_bank0.next = sync_fir_bank0
        sync_fir_N.next = system_fir_N
        fir_N.next = sync_fir_N
        sync_fcw.next = system_fcw
        fcw.next = sync_fcw
        sync_correct_i.next = system_correct_i
        correct_i.next = sync_correct_i
        sync_correct_q.next = system_correct_q
        correct_q.next = sync_correct_q
        sync_gain_i.next = system_gain_i
        gain_i.next = sync_gain_i
        sync_gain_q.next = system_gain_q
        gain_q.next = sync_gain_q
        sync_rx_correct_i.next = system_rx_correct_i
        rx_correct_i.next = sync_rx_correct_i
        sync_rx_correct_q.next = system_rx_correct_q
        rx_correct_q.next = sync_rx_correct_q

    interp_counter = Signal(intbv(0)[32:])
    done = Signal(bool(0))

    decim_counter = Signal(intbv(0)[32:])
    rx_done = Signal(bool(0))

    @always_seq(dac_clock.posedge, reset=clearn)
    def consumer():
        if txen:
            if interp_counter == 0:
                interp_counter.next = interp - 1
                if fifo_empty:
                    if txstop:
                        done.next = True
                        fifo_re.next = False
                    else:
                        underrun.next = underrun + 1
                        done.next = False
                        fifo_re.next = False
                else:
                    done.next = False
                    fifo_re.next = True
            else:
                interp_counter.next = interp_counter - 1
                fifo_re.next = False

    @always_seq(dac_clock.posedge, reset=clearn)
    def producer():
        if rxen and downsampled_valid:
            if rx_fifo_full:
                if rxstop:
                    rx_done.next = True
                    rx_fifo_we.next = False
                else:
                    rx_overrun.next = rx_overrun + 1
                    rx_done.next = False
                    rx_fifo_we.next = False
            else:
                rx_done.next = False
                rx_fifo_we.next = True
        else:
            rx_done.next = False
            rx_fifo_we.next = False

    @always_seq(dac_clock.posedge, reset=clearn)
    def sampler():
        if txen:
            if done:
                sample_i.next = 0
                sample_q.next = 0
                sample_valid.next = True
                sample_last.next = True
            elif fifo_dvld:
                sample_i.next = fifo_rdata[16:].signed()
                sample_q.next = fifo_rdata[32:16].signed()
                sample_valid.next = True
                sample_last.next = False
            else:
                sample_i.next = 0
                sample_q.next = 0
                sample_valid.next = False
                sample_last.next = False
        else:
            sample_i.next = 0
            sample_q.next = 0
            sample_valid.next = False
            sample_last.next = False

        if rxen and downsampled_valid:
            rx_fifo_wdata.next = concat(
                downsampled_q[9], downsampled_q[9],
                downsampled_q[9], downsampled_q[9], downsampled_q[9],
                downsampled_q[9], downsampled_q[10:],
                downsampled_i[9], downsampled_i[9],
                downsampled_i[9], downsampled_i[9], downsampled_i[9],
                downsampled_i[9], downsampled_i[10:])
        else:
            rx_fifo_wdata.next = 0
    
    return (synchronizer, consumer, producer, sampler, ) + duc_chain + ddc_chain
