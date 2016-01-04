"""
Radio Front End
***************

"""
from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        toVerilog

WHITEBOX_REGISTER_FILE = dict(
    WE_SAMPLE_ADDR     = 0x00,
    WE_STATUS_ADDR     = 0x04,
    WE_INTERP_ADDR     = 0x08,
    WE_FCW_ADDR        = 0x0c,
    WE_RUNS_ADDR       = 0x10,
    WE_THRESHOLD_ADDR  = 0x14,
    WE_CORRECTION_ADDR = 0x18,
    WE_AVAILABLE_ADDR  = 0x1c,
    WE_DEBUG_ADDR      = 0x20,
    WE_GAIN_ADDR       = 0x24,
    W_FIR_ADDR         = 0x28,

    WR_SAMPLE_ADDR     = 0x30,
    WR_STATUS_ADDR     = 0x34,
    WR_DECIM_ADDR      = 0x38,
    # reserved           0x3c,
    WR_RUNS_ADDR       = 0x40,
    WR_THRESHOLD_ADDR  = 0x44,
    WR_CORRECTION_ADDR = 0x48,
    WR_AVAILABLE_ADDR  = 0x4c,
    WR_DEBUG_ADDR      = 0xa0,
)
"""Peripheral register file addresses."""
for name, addr in WHITEBOX_REGISTER_FILE.iteritems():
    globals()[name] = addr

WHITEBOX_STATUS_REGISTER = dict(
    # BYTE 0 - CONTROL FLAGS - WRITE ONLY
    WS_CLEAR = 0,
    WS_LOOPEN = 1,
    # 4 - 7 reserved

    # BYTE 1 NIBBLE 1 - TX DSP FLAGS - R/W
    WES_TXEN = 8,
    WES_FILTEREN = 9,
    WES_DDSEN = 10,
    WES_TXSTOP = 11,

    # BYTE 1 NIBBLE 2 - TX FIFO FLAGS - READ ONLY
    WES_AEMPTY = 12,
    WES_AFULL = 13,
    WES_SPACE = 14,
    WES_DATA = 15,

    # BYTE 2 NIBBLE 1 - RX DSP FLAGS - R/W
    WRS_RXEN = 16,
    WRS_FILTEREN = 17,
    # 18 reserved
    WRS_RXSTOP = 19,

    # BYTE 2 NIBBLE 2 - RX FIFO FLAGS - READ ONLY
    WRS_AEMPTY = 20,
    WRS_AFULL = 21,
    WRS_SPACE = 22,
    WRS_DATA = 23,

    # BYTE 3 - COMBINED R/W FLAGS
    WS_FIREN = 24,
    # 25 - 31 reserved
)
"""Peripheral status register bit flags."""
for name, bit in WHITEBOX_STATUS_REGISTER.iteritems():
    globals()[name] = bit

WF_ACCESS_COEFFS = 16

def rfe(resetn,
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
        ddc_overrun, adc_last,
        **kwargs):
    """The Radio Front End glues together the APB3 interface and the
    DSP chains.  It consists of a synchronizer and a controller.

    The synchronizer safely transfers register data between the system and
    sample clock domains.

    The controller responds to read and write requests to the register file
    on the APB3 bus.

    :returns: A MyHDL synthesizable module
    """

    state_t = enum('IDLE', 'ACCESS', 'READ', 'READ2', 'READ_SAMPLE', 'READ_SAMPLE2', 'READ_SAMPLE3', 'READ_SAMPLE4', 'DONE',)
    state = Signal(state_t.IDLE)

    addr = Signal(intbv(0)[8:])

    # Local registers #
    overrun = Signal(modbv(0, min=0, max=2**16))
    underrun = Signal(modbv(0, min=0, max=2**16))
    counter = Signal(intbv(0)[len(prdata):])
    afval = Signal(intbv(0)[len(tx_fifo_afval):])
    aeval = Signal(intbv(0)[len(tx_fifo_aeval):])

    # Synchronizer registers #
    sync_clearn = Signal(bool(1))
    clear_ackn = Signal(bool(1))

    sync_tx_afull = Signal(bool(0))
    tx_afull = Signal(bool(0))
    sync_tx_aempty = Signal(bool(1))
    tx_aempty = Signal(bool(1))
    sync_tx_underrun = Signal(modbv(0, min=0, max=2**16))
    tx_underrun = Signal(modbv(0, min=0, max=2**16))
    sync_txlast = Signal(bool(0))
    txlast = Signal(bool(0))

    sync_rx_afull = Signal(bool(0))
    rx_afull = Signal(bool(0))
    sync_rx_aempty = Signal(bool(1))
    rx_aempty = Signal(bool(1))
    sync_rx_overrun = Signal(modbv(0, min=0, max=2**16))
    rx_overrun = Signal(modbv(0, min=0, max=2**16))
    sync_rxlast = Signal(bool(0))
    rxlast = Signal(bool(0))

    write_count = Signal(intbv(0)[32:])
    read_count = Signal(intbv(0)[32:])

    @always_seq(pclk.posedge, reset=resetn)
    def synchronizer():
        sync_clearn.next = clearn
        clear_ackn.next = sync_clearn

        sync_tx_afull.next = tx_fifo_afull
        tx_afull.next = sync_tx_afull
        sync_tx_aempty.next = tx_fifo_aempty
        tx_aempty.next = sync_tx_aempty
        sync_tx_underrun.next = duc_underrun
        tx_underrun.next = sync_tx_underrun
        sync_txlast.next = dac_last
        txlast.next = sync_txlast

        sync_rx_afull.next = rx_fifo_afull
        rx_afull.next = sync_rx_afull
        sync_rx_aempty.next = rx_fifo_aempty
        rx_aempty.next = sync_rx_aempty
        sync_rx_overrun.next = ddc_overrun
        rx_overrun.next = sync_rx_overrun
        sync_rxlast.next = adc_last
        rxlast.next = sync_rxlast

    len_fir_load_coeff_ram_din = len(fir_load_coeff_ram_din)
    len_fir_N = len(fir_N)
    fir_load_coeff_k = Signal(intbv(0, min=0, max=fir_N.max))
    fir_accessing = Signal(bool(0))
    fir_accessing_next = Signal(bool(0))

    @always_seq(pclk.posedge, reset=resetn)
    def coeff_controller():
        if state == state_t.IDLE:
            if psel and fir_accessing:
                fir_load_coeff_ram_blk.next = False
                fir_load_coeff_ram_addr.next = concat(fir_bank1, fir_bank0, fir_load_coeff_k)
                fir_load_coeff_ram_wen.next = not pwrite # active high to active low
                fir_load_coeff_ram_din.next = pwdata[18:]
            else:
                fir_load_coeff_ram_blk.next = True
        elif state == state_t.ACCESS:
            if psel and fir_accessing and not pwrite:
                fir_load_coeff_ram_blk.next = False # Pipelined read
            else:
                fir_load_coeff_ram_blk.next = True
        else:
            fir_load_coeff_ram_blk.next = True
    
    @always_seq(pclk.posedge, reset=resetn)
    def controller():
        #pslverr.next = 0
        tx_dmaready.next = not tx_fifo_full
        rx_dmaready.next = not rx_fifo_empty

        tx_status_led.next = txen or ddsen
        rx_status_led.next = rxen

        if not clear_ackn:
            clear_enable.next = False
            txen.next = 0
            txstop.next = 0
            ddsen.next = 0
            overrun.next = 0
            rxen.next = 0
            rxstop.next = 0
            underrun.next = 0
            write_count.next = 0
            fir_accessing.next = 0

        if txlast:
            txen.next = 0
            txstop.next = 0

        if rxlast:
            rxen.next = 0
            rxstop.next = 0

        if state == state_t.IDLE:
            if not psel:
                state.next = state_t.IDLE
            else:
                state.next = state_t.ACCESS
                addr.next = paddr[8:]
                pready.next = False
        if state == state_t.ACCESS:
            if fir_accessing:
                fir_load_coeff_k.next = fir_load_coeff_k + 1
                if pwrite:
                    state.next = state_t.DONE
                    pready.next = True
                else:
                    pready.next = False
                    state.next = state_t.READ
            else:
                if not pwrite and addr == WR_SAMPLE_ADDR:
                    pready.next = False
                    state.next = state_t.READ_SAMPLE
                else:
                    pready.next = True
                    state.next = state_t.DONE
                if pwrite:
                    if addr == WE_SAMPLE_ADDR:
                        write_count.next = write_count + 1
                        if tx_fifo_full:
                            overrun.next = overrun + 1
                        else:
                            tx_fifo_wdata.next = pwdata
                            tx_fifo_we.next = True
                    elif addr == WE_STATUS_ADDR:
                        if pwdata[WS_CLEAR]:
                            clear_enable.next = True
                        elif pwdata[WS_LOOPEN]:
                            loopen.next = pwdata[WS_LOOPEN]
                        elif pwdata[WES_TXSTOP]:
                            txstop.next = True
                        else:
                            txen.next = pwdata[WES_TXEN]
                            txfilteren.next = pwdata[WES_FILTEREN]
                            ddsen.next = pwdata[WES_DDSEN]
                            firen.next = pwdata[WS_FIREN]
                    elif addr == WE_INTERP_ADDR:
                        interp.next = pwdata[len(interp):]
                        shift.next = pwdata[len(shift)+16:16]
                    elif addr == WE_FCW_ADDR:
                        fcw.next = pwdata[len(fcw):]
                    elif addr == WE_THRESHOLD_ADDR:
                        tx_fifo_afval.next = pwdata[26:16]
                        tx_fifo_aeval.next = pwdata[10:]
                    elif addr == WE_CORRECTION_ADDR:
                        tx_correct_q.next = pwdata[26:16].signed()
                        tx_correct_i.next = pwdata[10:].signed()
                    elif addr == WE_GAIN_ADDR:
                        tx_gain_q.next = pwdata[26:16]
                        tx_gain_i.next = pwdata[10:]
                    elif addr == W_FIR_ADDR:
                        fir_bank1.next = pwdata[len_fir_N+2]
                        fir_bank0.next = pwdata[len_fir_N+1]
                        fir_N.next = pwdata[len_fir_N:]
                        #fir_accessing.next = pwdata[WF_ACCESS_COEFFS]
                        fir_accessing_next.next = pwdata[WF_ACCESS_COEFFS]
                        fir_load_coeff_k.next = 0
                    elif addr == WR_STATUS_ADDR:
                        if pwdata[WS_CLEAR]:
                            clear_enable.next = True
                        elif pwdata[WS_LOOPEN]:
                            loopen.next = pwdata[WS_LOOPEN]
                        elif pwdata[WRS_RXSTOP]:
                            rxstop.next = True
                        else:
                            rxen.next = pwdata[WRS_RXEN]
                            rxfilteren.next = pwdata[WRS_FILTEREN]
                    elif addr == WR_DECIM_ADDR:
                        decim.next = pwdata[len(decim):]
                    elif addr == WR_THRESHOLD_ADDR:
                        rx_fifo_afval.next = pwdata[26:16]
                        rx_fifo_aeval.next = pwdata[10:]
                    elif addr == WR_CORRECTION_ADDR:
                        rx_correct_q.next = pwdata[26:16].signed()
                        rx_correct_i.next = pwdata[10:].signed()
                else:
                    if addr == WE_STATUS_ADDR:
                        prdata.next = concat(
                            # BYTE 3 NIBBLE 2 - RESERVED
                            intbv(0)[4:],
                            # BYTE 3 NIBBLE 1 - COMBINED FLAGS
                            intbv(0)[3:], firen,
                            # BYTE 2 NIBBLE 2 - RX FIFO FLAGS
                            intbv(0)[4:],
                            # BYTE 2 NIBBLE 1 - RX DSP FLAGS
                            intbv(0)[4:],
                            # BYTE 1 NIBBLE 2 - TX FIFO FLAGS
                            not tx_fifo_empty, not tx_fifo_full,
                            tx_afull, tx_aempty,
                            # BYTE 1 NIBBLE 1 - TX DSP FLAGS
                            bool(0), ddsen, txfilteren, txen,
                            # BYTE 0 - RESERVED
                            intbv(0)[6:], loopen, not clearn)
                    elif addr == WE_INTERP_ADDR:
                        prdata.next = concat(
                            intbv(0)[16-len(shift):], shift,
                            intbv(0)[16-len(interp):], interp)
                    elif addr == WE_FCW_ADDR:
                        prdata.next = concat(intbv(0)[32-len(fcw):], fcw)
                    elif addr == WE_RUNS_ADDR:
                        prdata.next = concat(tx_underrun, overrun)
                    elif addr == WE_THRESHOLD_ADDR:
                        prdata.next = concat(intbv(0)[6:], tx_fifo_afval,
                            intbv(0)[6:], tx_fifo_aeval)
                    elif addr == WE_CORRECTION_ADDR:
                        prdata.next = concat(intbv(0)[6:], tx_correct_q,
                                intbv(0)[6:], tx_correct_i)
                    elif addr == WE_GAIN_ADDR:
                        prdata.next = concat(intbv(0)[6:], tx_gain_q,
                                intbv(0)[6:], tx_gain_i)
                    elif addr == WE_AVAILABLE_ADDR:
                        prdata.next = concat(intbv(0)[32-len(tx_fifo_wrcnt):], tx_fifo_wrcnt)
                    elif addr == WE_DEBUG_ADDR:
                        prdata.next = write_count
                    elif addr == W_FIR_ADDR:
                        prdata.next = concat(intbv(0)[32-len_fir_N-2:], fir_bank1, fir_bank0, fir_N)
                    elif addr == WR_SAMPLE_ADDR:
                        read_count.next = read_count + 1
                        if rx_fifo_empty:
                            underrun.next = underrun + 1
                        else:
                            rx_fifo_re.next = True
                    elif addr == WR_STATUS_ADDR:
                        prdata.next = concat(
                            # BYTE 3 NIBBLE 2 - RESERVED
                            intbv(0)[4:],
                            # BYTE 3 NIBBLE 1 - COMBINED FLAGS
                            intbv(0)[3:], firen,
                            # BYTE 2 NIBBLE 2 - RX FIFO FLAGS
                            not rx_fifo_empty, not rx_fifo_full,
                            rx_afull, rx_aempty,
                            # BYTE 2 NIBBLE 1 - RX DSP FLAGS
                            bool(0), bool(0), rxfilteren, rxen,
                            # BYTE 1 NIBBLE 2 - TX FIFO FLAGS
                            intbv(0)[4:],
                            # BYTE 1 NIBBLE 1 - TX DSP FLAGS
                            intbv(0)[4:],
                            # BYTE 0 - Misc Flags
                            intbv(0)[6:], loopen, not clearn)
                    elif addr == WR_DECIM_ADDR:
                        prdata.next = concat(intbv(0)[32-len(decim):], decim)
                    elif addr == WR_RUNS_ADDR:
                        prdata.next = concat(rx_overrun, underrun)
                    elif addr == WR_THRESHOLD_ADDR:
                        prdata.next = concat(intbv(0)[6:], rx_fifo_afval,
                            intbv(0)[6:], rx_fifo_aeval)
                    elif addr == WR_CORRECTION_ADDR:
                        prdata.next = concat(intbv(0)[6:], rx_correct_q,
                                intbv(0)[6:], rx_correct_i)
                    elif addr == WR_AVAILABLE_ADDR:
                        prdata.next = concat(intbv(0)[32-len(rx_fifo_rdcnt):], rx_fifo_rdcnt)
                    elif addr == WR_DEBUG_ADDR:
                        prdata.next = intbv(0)[32:]
                    else:
                        prdata.next = 0
        elif state == state_t.READ:
            pready.next = False
            state.next = state_t.READ2
        elif state == state_t.READ2:
            pready.next = True
            prdata.next = fir_load_coeff_ram_dout.signed()
            state.next = state_t.DONE
        elif state == state_t.READ_SAMPLE:
            pready.next = False
            rx_fifo_re.next = False
            state.next = state_t.READ_SAMPLE2
        elif state == state_t.READ_SAMPLE2:
            pready.next = False
            rx_fifo_re.next = False
            state.next = state_t.READ_SAMPLE3
        elif state == state_t.READ_SAMPLE3:
            pready.next = False
            rx_fifo_re.next = False
            state.next = state_t.READ_SAMPLE4
        elif state == state_t.READ_SAMPLE4:
            pready.next = True
            rx_fifo_re.next = False
            prdata.next = rx_fifo_rdata
            state.next = state_t.DONE
        elif state == state_t.DONE:
            tx_fifo_we.next = False
            rx_fifo_re.next = False
            pready.next = True
            state.next = state_t.IDLE
            fir_accessing_next.next = False
            if not fir_accessing:
                fir_accessing.next = fir_accessing_next
            elif fir_load_coeff_k == fir_N:
                fir_accessing.next = False

    return synchronizer, controller, coeff_controller

def print_rfe_ioctl():
    for name, addr in sorted(WHITEBOX_REGISTER_FILE.iteritems(),
            key=lambda x: x[1]):
        print "#define %-24s%s" % (name, hex(addr))
    for name, bit in sorted(WHITEBOX_STATUS_REGISTER.iteritems(),
            key=lambda x: x[1]):
        print "#define %-24s(1 << %d)" % (name, bit)
    print "#define %-24s(1 << %d)" % ("WF_ACCESS_COEFFS", WF_ACCESS_COEFFS)
