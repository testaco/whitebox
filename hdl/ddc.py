from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

from dsp import Signature, offset_corrector, binary_offseter
from dsp import iqmux

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
                out_q.next = out_q
                out_valid.next = True
                cnt.next = decim - 1
            else:
                out_i.next = 0
                out_q.next = 0
                out_valid.next = False
                cnt.next = cnt - 1
        else:
            out_i.next = 0
            out_q.next = 0
            out_valid.next = False
            out_last.next = False
            cnt.next = decim - 1

    return downsample

def ddc_fake(clearn, dac_clock,
            fifo_full, fifo_we, fifo_wdata,
            rxen, rxstop, filteren, decim, correct_i, correct_q,
            overrun, sample,
            adc_idata, adc_qdata, adc_last, **kwargs):

    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

    @always_seq(dac_clock.posedge, reset=clearn)
    def pass_through():
        overrun.next = 0
        fifo_we.next = 0

    return pass_through

def ddc(clearn, dac_clock,
        loopen, loopback,
        fifo_full, fifo_we, fifo_wdata,
        system_rxen, system_rxstop, system_filteren, 
        system_decim, system_correct_i, system_correct_q,
        overrun,
        adc_idata, adc_qdata, adc_last, **kwargs):
    """Direct Down Converter.

    :param clearn: The reset signal.
    :param dac_clock: The sampling clock.
    :param loopen: Loopback enable.
    :param loopback: Loopback signature.
    :param fifo_full: Is the receive FIFO full.
    :param fifo_we: Output to signal to FIFO to write.
    :param fifo_wdata: The sample to write to the FIFO.
    :param system_rxen: Enable the receiver.
    :param system_rxstop: Stop the receiver.
    :param system_filteren: Enable the receiver decimation filter.
    :param system_decim: Decimation factor.
    :param system_correct_i: i channel DC offset correction for the AQM.
    :param system_correct_q: q channel DC offset correction for the AQM.
    :param overrun: How many overruns have happened, signal to RFE.
    :param adc_idata: The input I data from the ADC.
    :param adc_qdata: The input Q data from the ADC.
    :param adc_last: Signifies that this is the last sample from the ADC.
    :returns: A synthesizable MyHDL instance.
    """
    dspsim = kwargs.get('dspsim', None)
    decim_default = kwargs.get('decim', 1)

    chain = []

    sync_rxen = Signal(bool(0))
    rxen = Signal(bool(0))
    sync_rxstop = Signal(bool(0))
    rxstop = Signal(bool(0))
    sync_decim = Signal(intbv(decim_default)[len(system_decim):])
    decim = Signal(intbv(decim_default)[len(system_decim):])
    sync_correct_i = Signal(intbv(0)[len(system_correct_i):])
    correct_i = Signal(intbv(0)[len(system_correct_i):])
    sync_correct_q = Signal(intbv(0)[len(system_correct_q):])
    correct_q = Signal(intbv(0)[len(system_correct_q):])
    we = Signal(bool(0))

    #if kwargs.get('conditioning_enable', True):
    #    offset = Signature("offset", True, bits=10)
    #    offseter = binary_offseter(clearn, dac_clock,
    #            sample, offset)
    #    chain.append(offseter)

    #    corrected = Signature("corrected", True, bits=10)
    #    corrector = offset_corrector(clearn, dac_clock,
    #            correct_i, correct_q,
    #            rf_out, corrected)
    #    chain.append(corrector)
    #else:
    adc = Signature("adc", True, bits=10, valid=rxen,
            i=adc_idata, q=adc_qdata)

    conditioned = Signature("conditioned", True, bits=10)
    loopback_0 = iqmux(clearn, dac_clock, loopen, adc, loopback,
            conditioned)
    chain.append(loopback_0)

    downsampled = Signature("downsampled", True, bits=10)
    downsampler_0 = downsampler(clearn, dac_clock, conditioned,
            downsampled, decim)
    chain.append(downsampler_0)

    rx = downsampled
    rx_i = rx.i
    rx_q = rx.q
    rx_valid = rx.valid
    rx_last = rx.last
    adc_last = rx_last

    @always_seq(dac_clock.posedge, reset=clearn)
    def synchronizer():
        sync_rxen.next = system_rxen
        rxen.next = sync_rxen
        sync_rxstop.next = system_rxstop
        rxstop.next = sync_rxstop
        sync_decim.next = system_decim
        decim.next = sync_decim
        sync_correct_i.next = system_correct_i
        correct_i.next = sync_correct_i
        sync_correct_q.next = system_correct_q
        correct_q.next = sync_correct_q

    @always_seq(dac_clock.posedge, reset=clearn)
    def producer():
        if rxen and rx_valid:
            # Sign extension
            fifo_wdata.next = concat(
                rx_q[10], rx_q[10], rx_q[10],
                rx_q[10], rx_q[10], rx_q[10],
                rx_q[10:],
                rx_i[10], rx_i[10], rx_i[10],
                rx_i[10], rx_i[10], rx_i[10],
                rx_i[10:])
            fifo_we.next = True
            we.next = True

            if we:
                fifo_we.next = False
                we.next = False
                if fifo_full and not rxstop:
                    overrun.next = overrun + 1

    return synchronizer, producer, chain
