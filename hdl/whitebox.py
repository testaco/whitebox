"""
Whitebox SoC Peripheral
=======================
"""
from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        toVerilog

from fir import fir as FIR
from dsp import Signature
from duc import duc as DUC
from rfe import rfe as RFE
from rfe import print_rfe_ioctl
from ram import Ram, Ram2

whitebox_config = dict(
        dsp_clock_rate=40e6,
        dac_sample_rate=10e6,
        default_sample_rate=200e3,
        #interp=50,
        rfe_enable=True,
        duc_enable=True,
        fir_enable=False,
        cic_enable=False,
        cic_order=4,
        cic_delay=1,
        dds_enable=False,
        conditioning_enable=False,
        ddc_enable=False)

class OverrunError(Exception):
    """Thrown when the system experiences an overflow on a FIFO buffer."""
    pass

class UnderrunError(Exception):
    """Thrown when the system experiences an underflow on a FIFO buffer."""
    pass

def whitebox_reset(resetn,
                  dsp_clock,
                  clear_enable,
                  clearn):

    state_t = enum('CLEAR', 'CLEARING', 'RUN')
    state = Signal(state_t.CLEAR)

    sync_clear_en = Signal(bool(0))
    clear_en = Signal(bool(0))
    clear_count = Signal(intbv(0)[10:])

    @always_seq(dsp_clock.posedge, reset=resetn)
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

def whitebox(
        resetn,
        pclk,
        paddr,
        psel,
        penable,
        pwrite,
        pwdata,
        pready,
        prdata,
        #pslverr,
        clearn,
        clear_enable,
        dac_clock,
        dsp_clock,
        dac_en,
        dac_data,
        adc_idata,
        adc_qdata,
        tx_status_led,
        tx_dmaready,
        rx_status_led,
        rx_dmaready,
        tx_fifo_re,
        tx_fifo_rdata,
        tx_fifo_we,
        tx_fifo_wdata,
        tx_fifo_full,
        tx_fifo_afull,
        tx_fifo_empty,
        tx_fifo_aempty,
        tx_fifo_afval,
        tx_fifo_aeval,
        tx_fifo_wack,
        tx_fifo_dvld,
        tx_fifo_overflow,
        tx_fifo_underflow,
        tx_fifo_rdcnt,
        tx_fifo_wrcnt,
        rx_fifo_re,
        rx_fifo_rdata,
        rx_fifo_we,
        rx_fifo_wdata,
        rx_fifo_full,
        rx_fifo_afull,
        rx_fifo_empty,
        rx_fifo_aempty,
        rx_fifo_afval,
        rx_fifo_aeval,
        rx_fifo_wack,
        rx_fifo_dvld,
        rx_fifo_overflow,
        rx_fifo_underflow,
        rx_fifo_rdcnt,
        rx_fifo_wrcnt,
        **kwargs):
    """The whitebox.

    :param resetn: Reset the whole radio front end.
    :param clearn: Clear the DSP Chain
    :param dac_clock: Clock running at DAC rate
    :param dsp_clock: Clock running at DSP rate (4x dac rate)
    :param pclk: The system bus clock
    :param paddr: The bus assdress
    :param psel: The bus slave select
    :param penable: The bus slave enable line
    :param pwrite: The bus read/write flag
    :param pwdata: The bus write data
    :param pready: The bus slave ready signal
    :param prdata: The bus read data
    :param pslverr: The bus slave error flag
    :param dac_data: The DAC data
    :param dac_en: Enable DAC output
    :param status_led: Output pin for whitebox status
    :param dmaready: Ready signal to DMA controller
    :param txirq: Almost empty interrupt to CPU
    :param clear_enable: To reset controller, set this high for reset
    """
    dspsim = kwargs.get('dspsim', None)
    dsp_clock_rate = kwargs.get('dsp_clock_rate', 40e6)
    dac_sample_rate = kwargs.get('dac_sample_rate', 10e6)
    default_sample_rate = kwargs.get('default_sample_rate', 200e3)
    # TODO CLOCK
    interp_default = kwargs.get('interp', int(dac_sample_rate / default_sample_rate))
    assert interp_default * default_sample_rate == dac_sample_rate
    whitebox_config['interp'] = interp_default
    fcw_bitwidth = kwargs.get('fcw_bitwidth', 25)

    ######### VARS AND FLAGS ###########
    print 'interp=', interp_default

    interp = Signal(intbv(interp_default)[11:])
    shift = Signal(intbv(0, min=0, max=21))
    firen = Signal(bool(0))
    fir_bank1 = Signal(bool(0))
    fir_bank0 = Signal(bool(0))
    fir_N = Signal(intbv(0, min=0, max=2**7))

    tx_correct_i = Signal(intbv(0, min=-2**9, max=2**9))
    tx_correct_q = Signal(intbv(0, min=-2**9, max=2**9))
    tx_gain_i = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    tx_gain_q = Signal(intbv(int(1.0 * 2**9 + .5))[10:])
    fcw = Signal(intbv(1)[fcw_bitwidth:])
    txen = Signal(bool(0))
    txstop = Signal(bool(0))
    txfilteren = Signal(bool(0))
    ddsen = Signal(bool(False))
    loopen = Signal(bool(False))

    decim = Signal(intbv(interp_default)[11:])
    rx_correct_i = Signal(intbv(0, min=-2**9, max=2**9))
    rx_correct_q = Signal(intbv(0, min=-2**9, max=2**9))
    rxen = Signal(bool(0))
    rxstop = Signal(bool(0))
    rxfilteren = Signal(bool(0))

    # The RAMs
    fir_coeff_ram = Ram(clearn, dsp_clock, bus.pclk, width=18, depth=512)
    fir_delay_line_i_ram = Ram(clearn, dsp_clock, dsp_clock, width=9, depth=512)
    fir_delay_line_q_ram = Ram(clearn, dsp_clock, dsp_clock, width=9, depth=512)

    fir_coeff_ram_addr = fir_coeff_ram.port['a'].addr
    fir_coeff_ram_din = fir_coeff_ram.port['a'].din
    fir_coeff_ram_blk = fir_coeff_ram.port['a'].blk
    fir_coeff_ram_wen = fir_coeff_ram.port['a'].wen
    fir_coeff_ram_dout = fir_coeff_ram.port['a'].dout
    fir_load_coeff_ram_addr = fir_coeff_ram.port['b'].addr
    fir_load_coeff_ram_din = fir_coeff_ram.port['b'].din
    fir_load_coeff_ram_blk = fir_coeff_ram.port['b'].blk
    fir_load_coeff_ram_wen = fir_coeff_ram.port['b'].wen
    fir_load_coeff_ram_dout = fir_coeff_ram.port['b'].dout
    fir_delay_line_i_ram_addr = fir_delay_line_i_ram.port['a'].addr
    fir_delay_line_i_ram_din = fir_delay_line_i_ram.port['a'].din
    fir_delay_line_i_ram_blk = fir_delay_line_i_ram.port['a'].blk
    fir_delay_line_i_ram_wen = fir_delay_line_i_ram.port['a'].wen
    fir_delay_line_i_ram_dout = fir_delay_line_i_ram.port['a'].dout
    fir_delay_line_q_ram_addr = fir_delay_line_q_ram.port['a'].addr
    fir_delay_line_q_ram_din = fir_delay_line_q_ram.port['a'].din
    fir_delay_line_q_ram_blk = fir_delay_line_q_ram.port['a'].blk
    fir_delay_line_q_ram_wen = fir_delay_line_q_ram.port['a'].wen
    fir_delay_line_q_ram_dout = fir_delay_line_q_ram.port['a'].dout

    ########### DIGITAL SIGNAL PROCESSING #######
    loopback = Signature("loopback", False, bits=10)
    duc_underrun = Signal(modbv(0, min=0, max=2**16))
    dac_last = Signal(bool(0))

    ddc_overrun = Signal(modbv(0, min=0, max=2**16))
    ddc_flags = Signal(intbv(0)[4:])
    adc_last = Signal(bool(0))

    tx_sample = Signature("tx_sample", True, bits=16)
    tx_sample_valid = tx_sample.valid
    tx_sample_last = tx_sample.last
    tx_sample_i = tx_sample.i
    tx_sample_q = tx_sample.q

    rx_sample = Signature("rx_sample", True, bits=16)
    rx_sample_valid = rx_sample.valid
    rx_sample_last = rx_sample.last
    rx_sample_i = rx_sample.i
    rx_sample_q = rx_sample.q

    duc_args = (clearn, dac_clock, dsp_clock,
            loopen, loopback,
            tx_fifo_empty, tx_fifo_re, tx_fifo_dvld, tx_fifo_rdata, tx_fifo_underflow,
            txen, txstop,
            ddsen, txfilteren,
            interp, shift,
            fcw,
            tx_correct_i, tx_correct_q,
            tx_gain_i, tx_gain_q,
            duc_underrun, tx_sample,
            dac_en, dac_data, dac_last,

            rx_fifo_full, rx_fifo_we, rx_fifo_wdata,
            rxen, rxstop, rxfilteren,
            decim, rx_correct_i, rx_correct_q,
            ddc_overrun, rx_sample,
            adc_idata, adc_qdata, adc_last,

            fir_coeff_ram_addr,
            fir_coeff_ram_din,
            fir_coeff_ram_blk,
            fir_coeff_ram_wen,
            fir_coeff_ram_dout,
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
            firen, fir_bank1, fir_bank0, fir_N,)

    duc_kwargs = dict(dspsim=dspsim,
                    interp=interp_default,
                    cic_enable=kwargs.get('cic_enable', True),
                    cic_order=kwargs.get('cic_order', 4),
                    cic_delay=kwargs.get('cic_delay', 1),
                    fir_enable=kwargs.get('fir_enable', True),
                    dds_enable=kwargs.get('dds_enable', True),
                    conditioning_enable=kwargs.get('conditioning_enable', True))
    if kwargs.get("duc_enable", True):
        duc = DUC(*duc_args, **duc_kwargs)
    else:
        duc = None

    ########### RADIO FRONT END ##############
    rfe_args = (resetn,
        pclk, paddr, psel, penable, pwrite, pwdata, pready, prdata, #pslverr,
        clearn, clear_enable, loopen,

        tx_status_led, tx_dmaready,
        rx_status_led, rx_dmaready,
        tx_fifo_we, tx_fifo_wdata,
        tx_fifo_empty, tx_fifo_full,
        tx_fifo_afval, tx_fifo_aeval, tx_fifo_afull, tx_fifo_aempty,
        tx_fifo_wack, tx_fifo_dvld,
        tx_fifo_overflow, tx_fifo_underflow,
        tx_fifo_rdcnt, tx_fifo_wrcnt,

        rx_fifo_re, rx_fifo_rdata,
        rx_fifo_empty, rx_fifo_full,
        rx_fifo_afval, rx_fifo_aeval, rx_fifo_afull, rx_fifo_aempty,
        rx_fifo_wack, rx_fifo_dvld,
        rx_fifo_overflow, rx_fifo_underflow,
        rx_fifo_rdcnt, rx_fifo_wrcnt,

        fir_load_coeff_ram_addr,
        fir_load_coeff_ram_din,
        fir_load_coeff_ram_blk,
        fir_load_coeff_ram_wen,
        fir_load_coeff_ram_dout,

        firen, fir_bank1, fir_bank0, fir_N,

        interp, shift,
        fcw, tx_correct_i, tx_correct_q, tx_gain_i, tx_gain_q,
        txen, txstop, ddsen, txfilteren,
        decim, rx_correct_i, rx_correct_q,
        rxen, rxstop, rxfilteren,
        duc_underrun, dac_last,
        ddc_overrun, adc_last)

    rfe = RFE(*rfe_args)

    instances = (rfe, duc)

    if kwargs.get('fir_enable', True):
        fir_coeff_ram_inst = fir_coeff_ram.instance_type()(**fir_coeff_ram.instance_signals())
        fir_delay_line_i_ram_inst = fir_delay_line_i_ram.instance_type()(**fir_delay_line_i_ram.instance_signals())
        fir_delay_line_q_ram_inst = fir_delay_line_q_ram.instance_type()(**fir_delay_line_q_ram.instance_signals())
        instances += (fir_coeff_ram_inst, fir_delay_line_i_ram_inst, fir_delay_line_q_ram_inst)


    return instances

if __name__ == '__main__':
    from apb3_utils import Apb3Bus
    from fifo import fifo as FIFO
    fifo_depth = 1024
    fifo_width = 32
    #duc_enable = True
    #rfe_enable = True

    clearn = ResetSignal(0, 0, async=True)
    clear_enable = Signal(bool(0))
    dsp_clock = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0)[10:])
    dac_en = Signal(bool(0))
    adc_idata = Signal(intbv(0, min=-2**9, max=2**9))
    adc_qdata = Signal(intbv(0, min=-2**9, max=2**9))
    tx_status_led = Signal(bool(0))
    tx_dmaready = Signal(bool(1))
    rx_status_led = Signal(bool(0))
    rx_dmaready = Signal(bool(1))

    bus = Apb3Bus()
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

    tx_fifo_re = Signal(bool(False))
    tx_fifo_rclk = Signal(bool(False))
    tx_fifo_rdata = Signal(intbv(0)[32:])
    tx_fifo_we = Signal(bool(False))
    tx_fifo_wclk = Signal(bool(False))
    tx_fifo_wdata = Signal(intbv(0)[32:])
    tx_fifo_full = Signal(bool(False))
    tx_fifo_afull = Signal(bool(False))
    tx_fifo_empty = Signal(bool(True))
    tx_fifo_aempty = Signal(bool(True))
    tx_fifo_afval = Signal(intbv(fifo_depth - 1)[10:])
    tx_fifo_aeval = Signal(intbv(0)[10:])
    tx_fifo_wack = Signal(bool(False))
    tx_fifo_dvld = Signal(bool(False))
    tx_fifo_overflow = Signal(bool(False))
    tx_fifo_underflow = Signal(bool(False))
    tx_fifo_rdcnt = Signal(intbv(0, min=0, max=fifo_depth + 1))
    tx_fifo_wrcnt = Signal(intbv(fifo_depth, min=0, max=fifo_depth + 1))

    tx_fifo_args = (
        clearn,
        tx_fifo_re,
        tx_fifo_rclk,
        tx_fifo_rdata,
        tx_fifo_we,
        tx_fifo_wclk,
        tx_fifo_wdata,
        tx_fifo_full,
        tx_fifo_afull,
        tx_fifo_empty,
        tx_fifo_aempty,
        tx_fifo_afval,
        tx_fifo_aeval,
        tx_fifo_wack,
        tx_fifo_dvld,
        tx_fifo_overflow,
        tx_fifo_underflow,
        tx_fifo_rdcnt,
        tx_fifo_wrcnt)
    tx_fifo = FIFO(*tx_fifo_args,
        width=fifo_width,
        depth=fifo_depth)

    rx_fifo_re = Signal(bool(False))
    rx_fifo_rclk = Signal(bool(False))
    rx_fifo_rdata = Signal(intbv(0)[32:])
    rx_fifo_we = Signal(bool(False))
    rx_fifo_wclk = Signal(bool(False))
    rx_fifo_wdata = Signal(intbv(0)[32:])
    rx_fifo_full = Signal(bool(False))
    rx_fifo_afull = Signal(bool(False))
    rx_fifo_empty = Signal(bool(True))
    rx_fifo_aempty = Signal(bool(True))
    rx_fifo_afval = Signal(intbv(fifo_depth - 1)[10:])
    rx_fifo_aeval = Signal(intbv(0)[10:])
    rx_fifo_wack = Signal(bool(False))
    rx_fifo_dvld = Signal(bool(False))
    rx_fifo_overflow = Signal(bool(False))
    rx_fifo_underflow = Signal(bool(False))
    rx_fifo_rdcnt = Signal(intbv(0, min=0, max=fifo_depth + 1))
    rx_fifo_wrcnt = Signal(intbv(fifo_depth, min=0, max=fifo_depth + 1))

    rx_fifo_args = (
        clearn,
        rx_fifo_re,
        rx_fifo_rclk,
        rx_fifo_rdata,
        rx_fifo_we,
        rx_fifo_wclk,
        rx_fifo_wdata,
        rx_fifo_full,
        rx_fifo_afull,
        rx_fifo_empty,
        rx_fifo_aempty,
        rx_fifo_afval,
        rx_fifo_aeval,
        rx_fifo_wack,
        rx_fifo_dvld,
        rx_fifo_overflow,
        rx_fifo_underflow,
        rx_fifo_rdcnt,
        rx_fifo_wrcnt)
    rx_fifo = FIFO(*rx_fifo_args,
        width=fifo_width,
        depth=fifo_depth)
    
    signals = (bus_presetn,
                bus_pclk,
                bus_paddr,
                bus_psel,
                bus_penable,
                bus_pwrite,
                bus_pwdata,
                bus_pready,
                bus_prdata,
                #bus_pslverr,

                clearn,
                clear_enable,
                dac_clock,
                dsp_clock,
                dac_en,
                dac_data,
                adc_idata,
                adc_qdata,
                tx_status_led,
                tx_dmaready,
                rx_status_led,
                rx_dmaready,

                tx_fifo_re,
                tx_fifo_rdata,
                tx_fifo_we,
                tx_fifo_wdata,
                tx_fifo_full,
                tx_fifo_afull,
                tx_fifo_empty,
                tx_fifo_aempty,
                tx_fifo_afval,
                tx_fifo_aeval,
                tx_fifo_wack,
                tx_fifo_dvld,
                tx_fifo_overflow,
                tx_fifo_underflow,
                tx_fifo_rdcnt,
                tx_fifo_wrcnt,

                rx_fifo_re,
                rx_fifo_rdata,
                rx_fifo_we,
                rx_fifo_wdata,
                rx_fifo_full,
                rx_fifo_afull,
                rx_fifo_empty,
                rx_fifo_aempty,
                rx_fifo_afval,
                rx_fifo_aeval,
                rx_fifo_wack,
                rx_fifo_dvld,
                rx_fifo_overflow,
                rx_fifo_underflow,
                rx_fifo_rdcnt,
                rx_fifo_wrcnt,
    )

    toVerilog(whitebox, *signals, **whitebox_config)

    toVerilog(whitebox_reset, bus_presetn,
            dsp_clock, clear_enable, clearn)

    print_rfe_ioctl()
