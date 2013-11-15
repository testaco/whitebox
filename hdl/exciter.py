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

from duc import duc as DUC
from duc import duc_fake
from duc import Signature

class OverrunError(Exception):
    pass

class UnderrunError(Exception):
    pass

WE_SAMPLE_ADDR     = 0x00
WE_STATE_ADDR      = 0x04
WE_INTERP_ADDR     = 0x08
WE_FCW_ADDR        = 0x0c
WE_RUNS_ADDR       = 0x10
WE_THRESHOLD_ADDR  = 0x14
WE_CORRECTION_ADDR = 0x18

WES_CLEAR_BIT = 0
WES_TXSTOP_BIT = 1
WES_TXEN_BIT = 8
WES_DDSEN_BIT = 9
WES_FILTEREN_BIT = 10
WES_LOOPEN_BIT = 11
WES_AEMPTY_BIT = 16
WES_AFULL_BIT = 17
WES_SPACE_BIT = 20
WES_DATA_BIT = 21

WES_CLEAR = intbv(2**WES_CLEAR_BIT)[32:]
WES_TXSTOP = intbv(2**WES_TXSTOP_BIT)[32:]
WES_TXEN = intbv(2**WES_TXEN_BIT)[32:]
WES_DDSEN = intbv(2**WES_DDSEN_BIT)[32:]
WES_FILTEREN = intbv(2**WES_FILTEREN_BIT)[32:]
WES_LOOPEN = intbv(2**WES_LOOPEN_BIT)[32:]
WES_AEMPTY = intbv(2**WES_AEMPTY_BIT)[32:]
WES_AFULL = intbv(2**WES_AFULL_BIT)[32:]
WES_SPACE = intbv(2**WES_SPACE_BIT)[32:]
WES_DATA = intbv(2**WES_DATA_BIT)[32:]

def duc_reset(resetn,
        dac_clock,
        clear_in,
        clearn):

    state_t = enum('RUN', 'CLEAR', 'CLEARING')
    state = Signal(state_t.IDLE)

    clear_count = Signal(intbv(0)[4:])

    @always_seq(dac_clock, reset=resetn)
    def reset():
        if state == state_t.RUN:
            clearn.next = 1
            if clear_in:
                state.next = state_t.CLEAR
        if state == state_t.CLEAR:
            clearn.next = 0
            clear_count = 16
            state.next = state_t.CLEARING
        elif state == state_t.CLEARING:
            clearn.next = clearn - 1
            if clearn == 0:
                state.next = state_t.RUN

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

    filteren = Signal(bool(0))

    ########## STATE MACHINE ######
    state_t = enum('IDLE', 'WRITE_SAMPLE', 'READ_FLAG', 'CLEAR', 'DONE',)
    state = Signal(state_t.IDLE)

    ########## DDS ################
    ddsen = Signal(bool(False))
    fcw = Signal(intbv(1)[32:])

    ############ TX EN ###########
    txen = Signal(bool(0))
    txstop = Signal(bool(0))

    sync_txlast = Signal(bool(0))
    txlast = Signal(bool(0))

    ########### DSP - FIFO STATUS #######
    duc_underrun = Signal(modbv(0, min=0, max=2**16))
    sync_underrun = Signal(modbv(0, min=0, max=2**16))
    underrun = Signal(modbv(0, min=0, max=2**16))
    overrun = Signal(modbv(0, min=0, max=2**16))

    sample = Signature("sample", True, bits=16)
    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

    dac_last = Signal(bool(0))
    duc_args = (clearn, dac_clock, dac2x_clock,
            fifo_empty, fifo_re, fifo_rdata,
            txen, txstop, ddsen, filteren, interp, fcw, correct_i, correct_q,
            duc_underrun, sample,
            dac_en, dac_data, dac_last,)

    duc_kwargs = dict(dspsim=dspsim, interp=interp_default)
    if kwargs.get("duc_enable", True):
        duc = DUC(*duc_args, **duc_kwargs)
    else:
        duc = duc_fake(*duc_args, **duc_kwargs)

    ## Copies of FIFO flags
    afull = Signal(bool(0))
    aempty = Signal(bool(1))
    sync_afull = Signal(bool(0))
    sync_aempty = Signal(bool(1))
    
    ##
    sync_prdata = Signal(intbv(0)[32:])
    clear_counter = Signal(intbv(0)[5:])

    @always_seq(pclk.posedge, reset=resetn)
    def synchronizer():
        sync_txlast.next = dac_last
        txlast.next = sync_txlast
        sync_underrun.next = duc_underrun
        underrun.next = sync_underrun
        sync_afull.next = fifo_afull
        afull.next = sync_afull
        sync_aempty.next = fifo_aempty
        aempty.next = sync_aempty

    @always_seq(pclk.posedge, reset=resetn)
    def state_machine():
        fifo_resetn.next = resetn and clearn
        dmaready.next = not afull
        txirq.next = aempty
        status_led.next = txen

        if txlast:
            txen.next = 0
            txstop.next = 0

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
                            txen.next = 0
                            ddsen.next = 0
                            filteren.next = 0
                            overrun.next = 0
                            clearn.next = 0
                            pready.next = 0
                            clear_counter.next = 3
                            state.next = state_t.CLEAR
                        elif pwdata[WES_TXSTOP_BIT]:
                            txstop.next = 1
                            pready.next = 0
                            state.next = state_t.DONE
                        else:
                            txen.next = pwdata[WES_TXEN_BIT]
                            ddsen.next = pwdata[WES_DDSEN_BIT]
                            filteren.next = pwdata[WES_FILTEREN_BIT]
                            pready.next = 0
                            state.next = state_t.DONE

                    else:
                        sync_prdata.next = concat(
                            # BYTE 3 - RESERVED
                            intbv(0)[8:],
                            # BYTE 2 NIBBLE 2 - FIFO DATA/SPACE
                            intbv(0)[2:], not fifo_empty, not fifo_full,
                            # BYTE 2 NIBBLE 1 - FIFO AFULL/AEMPTY
                            intbv(0)[2:], afull, aempty,
                            # BYTE 1 - DSP CHAIN
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
                        sync_prdata.next = interp
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_FCW_ADDR:
                    if pwrite:
                        fcw.next = pwdata
                        pready.next = 0
                        state.next = state_t.DONE
                    else:
                        sync_prdata.next = fcw
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_RUNS_ADDR:
                    if pwrite:
                        state.next = state_t.DONE
                    else:
                        sync_prdata.next = concat(overrun, underrun)
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == WE_THRESHOLD_ADDR:
                    if pwrite:
                        fifo_afval.next = pwdata[28:16]
                        fifo_aeval.next = pwdata[12:]
                        state.next = state_t.DONE
                    else:
                        sync_prdata.next = concat(intbv(0)[4:], fifo_afval,
                                intbv(0)[4:], fifo_aeval)
                        pready.next = 0
                        state.next = state_t.READ_FLAG
                        
                elif paddr[8:] == WE_CORRECTION_ADDR:
                    if pwrite:
                        correct_q.next = pwdata[26:16].signed()
                        correct_i.next = pwdata[10:].signed()
                        state.next = state_t.DONE
                    else:
                        sync_prdata.next = concat(intbv(0)[6:], correct_q[10:],
                                intbv(0)[6:], correct_i[10:])
                        pready.next = 0
                        state.next = state_t.READ_FLAG


        elif state == state_t.WRITE_SAMPLE:
            fifo_we.next = 1
            state.next = state_t.DONE

        elif state == state_t.READ_FLAG:
            prdata.next = sync_prdata
            state.next = state_t.DONE

        elif state == state_t.CLEAR:
            if clear_counter == 0:
                state.next = state_t.DONE
            else:
                clear_counter.next = clear_counter - 1

        elif state == state_t.DONE:
            fifo_we.next = 0
            pready.next = 1
            clearn.next = 1
            state.next = state_t.IDLE

    return synchronizer, state_machine, duc


if __name__ == '__main__':
    from apb3_utils import Apb3Bus
    from fifo import fifo as FIFO
    bus = Apb3Bus()
    fifo_depth = 1024
    fifo_width = 32
    interp = 20
    duc_enable = True

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

    toVerilog(exciter, *signals,
            interp=interp,
            duc_enable=duc_enable)

    print "#define WES_CLEAR\t%#010x" % WES_CLEAR
    print "#define WES_TXSTOP\t%#010x" % WES_TXSTOP
    print "#define WES_TXEN\t%#010x" % WES_TXEN
    print "#define WES_DDSEN\t%#010x" % WES_DDSEN
    print "#define WES_FILTEREN\t%#010x" % WES_FILTEREN
    print "#define WES_LOOPEN\t%#010x" % WES_LOOPEN
    print "#define WES_AEMPTY\t%#010x" % WES_AEMPTY
    print "#define WES_AFULL\t%#010x" % WES_AFULL
    print "#define WES_SPACE\t%#010x" % WES_SPACE
    print "#define WES_DATA\t%#010x" % WES_DATA

