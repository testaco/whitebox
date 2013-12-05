from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

from dsp import Signature, offset_corrector, binary_offseter
from dsp import iqmux

def downsampler(clearn, clock, in_sign, out_sign, decim):
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
        overrun, sample,
        adc_idata, adc_qdata, adc_last, **kwargs):
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

    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

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
    corrected = sample

    conditioned = Signature("conditioned", True, bits=10)
    loopback_0 = iqmux(clearn, dac_clock, loopen, corrected, loopback,
            conditioned)
    chain.append(loopback_0)

    downsampled = Signature("downsampled", True, bits=10)
    downsampler_0 = downsampler(clearn, dac_clock, conditioned,
            downsampled, decim)
    chain.append(downsampler_0)

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

    decim_counter = Signal(intbv(0)[32:])

    @always_seq(dac_clock.posedge, reset=clearn)
    def producer():
        if rxen and sample_valid:
            if decim_counter == 0:
                decim_counter.next = decim - 1
                # Sign extension
                fifo_wdata.next = concat(
                    sample_q[10], sample_q[10], sample_q[10],
                    sample_q[10], sample_q[10], sample_q[10],
                    sample_q[10:],
                    sample_i[10], sample_i[10], sample_i[10],
                    sample_i[10], sample_i[10], sample_i[10],
                    sample_i[10:])
                fifo_we.next = True
                we.next = True
            else:
                decim_counter.next = decim_counter - 1

            if we:
                if fifo_full and not rxstop:
                    overrun.next = overrun + 1
                fifo_we.next = False
                we.next = False

    return synchronizer, producer, chain
