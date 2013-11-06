"""
Exciter
=======

The exciter works like so:
    1. Sensors, like the 1-bit key, or the 10-bit adc from microphone
       even Memory!  Perhaps always memory?  It always comes at a bitrate.
    2. Encoders, that take data and convert them into symbols at a baud rate.
    3. Right now, everything is quadrature data, I[n] & Q[n].  These are 16 bit fixed
       point numbers.
    4. The mode outputs, always I[n] & Q[n] are fed to the DAC, which has analog errors.
    5. A filter is applied to the outputted I[n] & Q[n] analog signals.
    6. The analog radio:
        1. Quadrature Modulates the signal with the carrier f_if
            I[n]*cos(2*pi*f_if) + Q[n]*sin(2*pi*f_if)
        2. It then filters the output
        3. It then Image-Reject upconverts with the f_rf
    7. The frontend then:
        1. goes through a matching balun network
        2. into a filter
        3. then an amplifier
        4. then an antenna

"""
from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        toVerilog

from dds import dds as DDS
from duc import *

class OverrunError(Exception):
    pass

class UnderrunError(Exception):
    pass

DDS_NUM_SAMPLES=512

WE_SAMPLE_ADDR     = 0x00
WE_STATE_ADDR      = 0x04
WE_INTERP_ADDR     = 0x08
WE_FCW_ADDR        = 0x0c
WE_RUNS_ADDR       = 0x10
WE_THRESHOLD_ADDR  = 0x14
WE_CORRECTION_ADDR = 0x18

WES_CLEAR_BIT = 0
WES_TXEN_BIT = 8
WES_DDSEN_BIT = 9
WES_FILTEREN_BIT = 10
WES_LOOPEN_BIT = 11
WES_AEMPTY_BIT = 16
WES_AFULL_BIT = 17

WES_CLEAR = intbv(2**WES_CLEAR_BIT)[32:]
WES_TXEN = intbv(2**WES_TXEN_BIT)[32:]
WES_DDSEN = intbv(2**WES_DDSEN_BIT)[32:]
WES_FILTEREN = intbv(2**WES_FILTEREN_BIT)[32:]
WES_LOOPEN = intbv(2**WES_LOOPEN_BIT)[32:]
WES_AEMPTY = intbv(2**WES_AEMPTY_BIT)[32:]
WES_AFULL = intbv(2**WES_AFULL_BIT)[32:]

def exciter(
        resetn,
        dac2x_clock,
        pclk,
        paddr,
        psel,
        penable,
        pwrite,
        pwdata,
        pready,
        prdata,
        pslverr,
        dac_clock,
        dac_data,
        dac_en,
        status_led,
        dmaready,
        txirq,
        fifo_resetn,
        fifo_re,
        fifo_rclk,
        fifo_rdata,
        fifo_we,
        fifo_wclk,
        fifo_wdata,
        fifo_full,
        fifo_afull,
        fifo_empty,
        fifo_aempty,
        fifo_afval,
        fifo_aeval,
        **kwargs):
    """The exciter.

    :param resetn: Reset
    :param dac_clock: Clock running at DAC rate
    :param dac2x_clock: Clock running at double DAC rate
    :param pclk: The system bus clock
    :param paddr: The bus assdress
    :param psel: The bus slave select
    :param penable: The bus slave enable line
    :param pwrite: The bus read/write flag
    :param pwdata: The bus write data
    :param pready: The bus slave ready signal
    :param prdata: The bus read data
    :param pslverr: The bus slave error flag
    :param dac_clock: The DAC clock
    :param dac_data: The DAC data
    :param dac_en: Enable DAC output
    :param status_led: Output pin for exciter status
    :param dmaready: Ready signal to DMA controller
    :param txirq: Almost empty interrupt to CPU
    """
    dspsim = kwargs.get('dspsim', None)
    interp_default = kwargs.get('interp', 1)

    clearn = ResetSignal(1, 0, async=False)

    correct_i = Signal(intbv(0, min=-2**9, max=2**9))
    correct_q = Signal(intbv(0, min=-2**9, max=2**9))

    ######### RESAMPLER ###########
    interp = Signal(intbv(interp_default)[32:])
    print 'interp=', interp
    interp_counter = Signal(intbv(0)[32:])

    filteren = Signal(bool(0))

    ########## STATE MACHINE ######
    state_t = enum('IDLE', 'WRITE_SAMPLE', 'READ_FLAG', 'CLEAR', 'DONE',)
    state = Signal(state_t.IDLE)

    ########## DDS ################
    ddsen = Signal(bool(False))
    dds_sample = Signal(intbv(0, min=-2**9, max=2**9))
    fcw = Signal(intbv(1)[32:])
    dds_args = resetn, dac_clock, ddsen, dds_sample, fcw
    dds = DDS(*dds_args, num_samples=DDS_NUM_SAMPLES)

    ############ TX EN ###########
    txen = Signal(bool(0))
    #underrun = Signal(bool(False))
    #overrun = Signal(bool(False))
    underrun = Signal(modbv(0, min=0, max=2**16))
    overrun = Signal(modbv(0, min=0, max=2**16))

    # DUC DSP Chain
    sample = Signature("sample", True, bits=16)
    sample_valid = sample.valid
    sample_i = sample.i
    sample_q = sample.q

    truncated_0 = Signature("truncated", True, bits=8)
    truncator_0 = truncator(clearn, dac_clock, sample, truncated_0)

    truncated_1 = Signature("truncated", True, bits=10)
    truncator_1 = truncator(clearn, dac_clock, sample, truncated_1)

    upsampled = Signature("upsampled", True, bits=10)
    upsampler_0 = upsampler(clearn, dac_clock, truncated_1, upsampled, interp)

    filtered = Signature("filtered", True, bits=10)
    cic_0 = cic(clearn, dac_clock, truncated_0, filtered,
            interp,
            cic_order=3, cic_delay=1,
            sim=dspsim)

    processed = Signature("processed", True, bits=10)
    processed_mux = iqmux(clearn, dac_clock,
            filteren,
            upsampled.valid, upsampled.i, upsampled.q,
            filtered.valid, filtered.i, filtered.q,
            processed.valid, processed.i, processed.q)

    muxed = Signal(bool(0))
    muxed_i = Signal(intbv(0, min=-2**9, max=2**9))
    muxed_q = Signal(intbv(0, min=-2**9, max=2**9))
    dds_mux = iqmux(clearn, dac_clock,
            ddsen,
            #filtered.valid, filtered.i, filtered.q,
            processed.valid, processed.i, processed.q,
            ddsen, dds_sample, 0, # dds_sample
            muxed, muxed_i, muxed_q)

    ## DAC Conditioning
    corrected = Signal(bool(0))
    corrected_i = Signal(intbv(0, min=-2**9, max=2**9))
    corrected_q = Signal(intbv(0, min=-2**9, max=2**9))
    corrector = offset_corrector(clearn, dac_clock,
            correct_i, correct_q,
            muxed, muxed_i, muxed_q,
            corrected, corrected_i, corrected_q)
    
    offset_valid = Signal(bool(0))
    offset_i = Signal(intbv(0, min=0, max=2**10))
    offset_q = Signal(intbv(0, min=0, max=2**10))
    offseter = binary_offseter(clearn, dac_clock,
            corrected, corrected_i, corrected_q,
            offset_valid, offset_i, offset_q)

    interleaver_0 = interleaver(clearn, dac_clock, dac2x_clock,
            offset_valid, offset_i, offset_q,
            dac_en, dac_data)

    @always_seq(pclk.posedge, reset=resetn)
    def state_machine():
        fifo_resetn.next = resetn and clearn
        dmaready.next = not fifo_afull
        txirq.next = fifo_aempty
        status_led.next = txen

        if state == state_t.IDLE:
            if penable and psel:
                if paddr[8:] == WE_SAMPLE_ADDR:
                    if pwrite:
                        if fifo_full:
                            overrun.next = overrun + 1
                            state.next = state_t.DONE
                        else:
                            state.next = state_t.WRITE_SAMPLE
                            pready.next = 0
                            fifo_wdata.next = pwdata

                elif paddr[8:] == WE_STATE_ADDR:
                    if pwrite:
                        if pwdata[WES_CLEAR_BIT]:
                            # Whats next... I think it's to try it with these
                            # new lines and see why... how TXEN is set,
                            # the FIFO is full, and the DMA is stuck!? weird
                            txen.next = 0
                            ddsen.next = 0
                            filteren.next = 0
                            overrun.next = 0
                            clearn.next = 0
                            pready.next = 0
                            state.next = state_t.CLEAR
                        else:
                            txen.next = pwdata[WES_TXEN_BIT]
                            ddsen.next = pwdata[WES_DDSEN_BIT]
                            filteren.next = pwdata[WES_FILTEREN_BIT]
                            pready.next = 0
                            state.next = state_t.DONE

                    else:
                        prdata.next = concat(
                            # BYTE 3 - RESERVED
                            intbv(0)[8:],
                            # BYTE 2 - FIFO STATUS
                            intbv(0)[6:], fifo_afull, fifo_aempty,
                            # BYTE 1 - DSP CHAIN STATUS
                            intbv(0)[5:], filteren, ddsen, txen,
                            # BYTE 0 - RESERVED
                            intbv(0)[8:])
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_INTERP_ADDR:
                    if pwrite:
                        interp.next = pwdata
                        pready.next = 0
                        state.next = state_t.DONE
                    else:
                        prdata.next = interp
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_FCW_ADDR:
                    if pwrite:
                        fcw.next = pwdata
                        pready.next = 0
                        state.next = state_t.DONE
                    else:
                        prdata.next = fcw
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_RUNS_ADDR:
                    if pwrite:
                        state.next = state_t.DONE
                    else:
                        prdata.next = concat(overrun, underrun)
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_THRESHOLD_ADDR:
                    if pwrite:
                        fifo_afval.next = pwdata[28:16]
                        fifo_aeval.next = pwdata[12:]
                        state.next = state_t.DONE
                    else:
                        prdata.next = concat(intbv(0)[4:], fifo_afval,
                                intbv(0)[4:], fifo_aeval)
                        pready.next = 0
                        state.next = state_t.READ_FLAG
                        
                elif paddr[8:] == WE_CORRECTION_ADDR:
                    if pwrite:
                        correct_q.next = pwdata[26:16].signed()
                        correct_i.next = pwdata[10:].signed()
                        state.next = state_t.DONE
                    else:
                        prdata.next = concat(intbv(0)[6:], correct_q[10:],
                                intbv(0)[6:], correct_i[10:])
                        pready.next = 0
                        state.next = state_t.READ_FLAG


        elif state == state_t.WRITE_SAMPLE:
            fifo_we.next = 1
            state.next = state_t.DONE

        elif state == state_t.READ_FLAG:
            state.next = state_t.DONE

        elif state == state_t.CLEAR:
            state.next = state_t.DONE

        elif state == state_t.DONE:
            fifo_we.next = 0
            pready.next = 1
            clearn.next = 1
            state.next = state_t.IDLE

    @always_seq(dac_clock.posedge, reset=clearn)
    def feeder():
        if txen:
            if interp_counter == 0:
                interp_counter.next = interp - 1
                # Watch for underrun
                if fifo_empty:
                    underrun.next = underrun + 1
                else:
                    fifo_re.next = True
            else:
                interp_counter.next = interp_counter - 1

            if fifo_re:
                sample_i.next = fifo_rdata[16:].signed()
                sample_q.next = fifo_rdata[32:16].signed()
                sample_valid.next = True
                fifo_re.next = False
            else:
                sample_valid.next = False
    
    return state_machine, feeder, dds, \
        truncator_0, truncator_1, cic_0, upsampler_0, processed_mux, \
        dds_mux, corrector, offseter, interleaver_0


if __name__ == '__main__':
    from apb3_utils import Apb3Bus
    from fifo import fifo as FIFO
    bus = Apb3Bus()
    fifo_depth = 1024
    fifo_width = 32
    interp = 20

    dac2x_clock = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0)[10:])
    dac_en = Signal(bool(0))
    status_led = Signal(bool(0))
    dmaready = Signal(bool(1))
    txirq = Signal(bool(0))

    bus_presetn = bus.presetn
    bus_pclk = bus.pclk
    bus_paddr = bus.paddr
    bus_psel = bus.psel
    bus_penable = bus.penable
    bus_pwrite = bus.pwrite
    bus_pwdata = bus.pwdata
    bus_pslverr = bus.pslverr
    bus_pready = bus.pready
    bus_prdata = bus.prdata

    fifo_re = Signal(bool(False))
    fifo_rclk = Signal(bool(False))
    fifo_rdata = Signal(intbv(0)[32:])
    fifo_we = Signal(bool(False))
    fifo_wclk = Signal(bool(False))
    fifo_wdata = Signal(intbv(0)[32:])
    fifo_full = Signal(bool(False))
    fifo_afull = Signal(bool(False))
    fifo_empty = Signal(bool(False))
    fifo_aempty = Signal(bool(False))
    fifo_afval = Signal(intbv(fifo_depth)[12:])
    fifo_aeval = Signal(intbv(0)[12:])
    fifo_resetn = Signal(bool(1))

    fifo_args = (
        fifo_resetn,
        fifo_re,
        fifo_rclk,
        fifo_rdata,
        fifo_we,
        fifo_wclk,
        fifo_wdata,
        fifo_full,
        fifo_afull,
        fifo_empty,
        fifo_aempty,
        fifo_afval,
        fifo_aeval)
    fifo = FIFO(*fifo_args,
        width=fifo_width,
        depth=fifo_depth)
    
    signals = (bus_presetn,
                dac2x_clock,
                bus_pclk,
                bus_paddr,
                bus_psel,
                bus_penable,
                bus_pwrite,
                bus_pwdata,
                bus_pready,
                bus_prdata,
                bus_pslverr,
                dac_clock,
                dac_data,
                dac_en,
                status_led,
                dmaready,
                txirq,
                fifo_resetn,
                fifo_re,
                fifo_rclk,
                fifo_rdata,
                fifo_we,
                fifo_wclk,
                fifo_wdata,
                fifo_full,
                fifo_afull,
                fifo_empty,
                fifo_aempty,
                fifo_afval,
                fifo_aeval)

    toVerilog(exciter, *signals, interp=interp)

    print "#define WES_CLEAR\t%#010x" % WES_CLEAR
    print "#define WES_TXEN\t%#010x" % WES_TXEN
    print "#define WES_DDSEN\t%#010x" % WES_DDSEN
    print "#define WES_FILTEREN\t%#010x" % WES_FILTEREN
    print "#define WES_LOOPEN\t%#010x" % WES_LOOPEN
    print "#define WES_AEMPTY\t%#010x" % WES_AEMPTY
    print "#define WES_AFULL\t%#010x" % WES_AFULL

