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
from rfe import rfe as RFE
from rfe import rfe_fake
from rfe import WES_CLEAR, WES_TXSTOP, WES_TXEN, WES_DDSEN, WES_FILTEREN, \
        WES_AEMPTY, WES_AFULL, WES_SPACE, WES_DATA

class OverrunError(Exception):
    pass

class UnderrunError(Exception):
    pass

def exciter_reset(resetn,
                  dac_clock,
                  clear_enable,
                  clearn):

    state_t = enum('CLEAR', 'CLEARING', 'RUN')
    state = Signal(state_t.CLEAR)

    sync_clear_en = Signal(bool(0))
    clear_en = Signal(bool(0))
    clear_count = Signal(intbv(0)[10:])

    @always_seq(dac_clock.posedge, reset=resetn)
    def controller():
        sync_clear_en.next = clear_enable
        clear_en.next = sync_clear_en
        if state == state_t.CLEAR:
            clearn.next = 0
            clear_count.next = 16
            state.next = state_t.CLEARING
        elif state == state_t.CLEARING:
            clear_count.next = clear_count - 1
            if clear_count == 0:
                state.next = state_t.RUN
            else:
                state.next = state_t.CLEARING
        if state == state_t.RUN:
            clearn.next = 1
            if clear_en:
                state.next = state_t.CLEAR
    
    return controller

def exciter(
        resetn,
        clearn,
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
        clear_enable,
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

    :param resetn: Reset the whole radio front end.
    :param clearn: Clear the DSP Chain
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
    :param clear_enable: To reset controller, set this high for reset
    """
    dspsim = kwargs.get('dspsim', None)
    interp_default = kwargs.get('interp', 1)

    ######### VARS AND FLAGS ###########
    interp = Signal(intbv(interp_default)[32:])
    correct_i = Signal(intbv(0, min=-2**9, max=2**9))
    correct_q = Signal(intbv(0, min=-2**9, max=2**9))
    fcw = Signal(intbv(1)[32:])
    print 'interp=', interp

    filteren = Signal(bool(0))
    ddsen = Signal(bool(False))
    txen = Signal(bool(0))
    txstop = Signal(bool(0))

    ########### DIGITAL SIGNAL PROCESSING #######
    duc_underrun = Signal(modbv(0, min=0, max=2**16))
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

    duc_kwargs = dict(dspsim=dspsim,
                    interp=interp_default,
                    cic_enable=kwargs.get('cic_enable', True),
                    dds_enable=kwargs.get('dds_enable', True),
                    conditioning_enable=kwargs.get('conditioning_enable', True))
    if kwargs.get("duc_enable", True):
        duc = DUC(*duc_args, **duc_kwargs)
    else:
        duc = duc_fake(*duc_args, **duc_kwargs)

    ########### RADIO FRONT END ##############
    rfe_args = (resetn, clearn, pclk,
            paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr,
            fifo_empty, fifo_full, fifo_we, fifo_wdata,
            fifo_afval, fifo_aeval, fifo_afull, fifo_aempty,
            txen, txstop, ddsen, filteren, interp, fcw, correct_i, correct_q,
            duc_underrun, dac_last,
            overrun, dmaready, status_led, clear_enable, txirq)

    if kwargs.get("rfe_enable", True):
        rfe = RFE(*rfe_args)
    else:
        rfe = rfe_fake(*rfe_args)

    return rfe, duc

if __name__ == '__main__':
    from apb3_utils import Apb3Bus
    from fifo import fifo as FIFO
    bus = Apb3Bus()
    fifo_depth = 1024
    fifo_width = 32
    interp = 20
    duc_enable = True
    rfe_enable = True

    clearn = ResetSignal(0, 0, async=True)
    dac2x_clock = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0)[10:])
    dac_en = Signal(bool(0))
    status_led = Signal(bool(0))
    dmaready = Signal(bool(1))
    txirq = Signal(bool(0))
    clear_enable = Signal(bool(0))

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

    fifo_args = (
        clearn,
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
                clearn,
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
                clear_enable,
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
            rfe_enable=True,
            duc_enable=True,
            cic_enable=False,
            dds_enable=False,
            conditioning_enable=False)

    toVerilog(exciter_reset, bus_presetn,
            dac_clock, clear_enable, clearn)

    print "#define WES_CLEAR\t%#010x" % WES_CLEAR
    print "#define WES_TXSTOP\t%#010x" % WES_TXSTOP
    print "#define WES_TXEN\t%#010x" % WES_TXEN
    print "#define WES_DDSEN\t%#010x" % WES_DDSEN
    print "#define WES_FILTEREN\t%#010x" % WES_FILTEREN
    print "#define WES_AEMPTY\t%#010x" % WES_AEMPTY
    print "#define WES_AFULL\t%#010x" % WES_AFULL
    print "#define WES_SPACE\t%#010x" % WES_SPACE
    print "#define WES_DATA\t%#010x" % WES_DATA

