from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        toVerilog

WE_SAMPLE_ADDR     = 0x00
WE_STATE_ADDR      = 0x04
WE_INTERP_ADDR     = 0x08
WE_FCW_ADDR        = 0x0c
WE_RUNS_ADDR       = 0x10
WE_THRESHOLD_ADDR  = 0x14
WE_CORRECTION_ADDR = 0x18

WES_CLEAR_BIT = 0
WES_TXSTOP_BIT = 1
WES_RXSTOP_BIT = 2
WES_TXEN_BIT = 8
WES_DDSEN_BIT = 9
WES_FILTEREN_BIT = 10
WES_RXEN_BIT = 12
WES_AEMPTY_BIT = 16
WES_AFULL_BIT = 17
WES_SPACE_BIT = 20
WES_DATA_BIT = 21

WES_CLEAR = intbv(2**WES_CLEAR_BIT)[32:]
WES_TXSTOP = intbv(2**WES_TXSTOP_BIT)[32:]
WES_RXSTOP = intbv(2**WES_RXSTOP_BIT)[32:]
WES_TXEN = intbv(2**WES_TXEN_BIT)[32:]
WES_DDSEN = intbv(2**WES_DDSEN_BIT)[32:]
WES_FILTEREN = intbv(2**WES_FILTEREN_BIT)[32:]
WES_RXEN = intbv(2**WES_RXEN_BIT)[32:]
WES_AEMPTY = intbv(2**WES_AEMPTY_BIT)[32:]
WES_AFULL = intbv(2**WES_AFULL_BIT)[32:]
WES_SPACE = intbv(2**WES_SPACE_BIT)[32:]
WES_DATA = intbv(2**WES_DATA_BIT)[32:]

def rfe_fake(resetn, clearn, pclk,
        paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr,
        fifo_empty, fifo_full, fifo_we, fifo_wdata,
        fifo_afval, fifo_aeval, fifo_afull, fifo_aempty,
        txen, txstop, ddsen, filteren, interp, fcw, correct_i, correct_q,
        duc_underrun, dac_last,
        overrun, dmaready, status_led, clear_enable, txirq):

    state_t = enum('IDLE', 'ACCESS', 'DONE',)
    state = Signal(state_t.IDLE)

    addr = Signal(intbv(0)[8:])

    # Local registers #
    counter = Signal(intbv(0)[len(prdata):])

    # Synchronizer registers #
    sync_clearn = Signal(bool(1))
    clear_ackn = Signal(bool(1))
    sync_txlast = Signal(bool(0))
    txlast = Signal(bool(0))

    @always_seq(pclk.posedge, reset=resetn)
    def synchronizer():
        sync_clearn.next = clearn
        clear_ackn.next = sync_clearn
        sync_txlast.next = dac_last
        txlast.next = sync_txlast
    
    @always_seq(pclk.posedge, reset=resetn)
    def controller():
        #pslverr.next = 0
        txirq.next = 0
        dmaready.next = 1

        status_led.next = txen

        if clear_enable:
            txen.next = 0
            txstop.next = 0
            ddsen.next = 0
            filteren.next = 0
            overrun.next = 0

        if not clear_ackn:
            clear_enable.next = False

        if txlast:
            txen.next = 0
            txstop.next = 0

        if state == state_t.IDLE:
            if not psel:
                state.next = state_t.IDLE
            else:
                state.next = state_t.ACCESS
                addr.next = paddr[8:]
                pready.next = 0
        if state == state_t.ACCESS:
            pready.next = 0
            state.next = state_t.DONE
            if pwrite:
                if addr == WE_INTERP_ADDR:
                    interp.next = pwdata
                elif addr == WE_FCW_ADDR:
                    fcw.next = pwdata
            else:
                if addr == WE_INTERP_ADDR:
                    prdata.next = interp
                elif addr == WE_FCW_ADDR:
                    prdata.next = fcw
        elif state == state_t.DONE:
            fifo_we.next = False
            pready.next = True
            state.next = state_t.IDLE

    return synchronizer, controller


def rfe(resetn,
        pclk, paddr, psel, penable, pwrite, pwdata, pready, prdata, #pslverr,
        clearn, clear_enable,
        tx_status_led, tx_dmaready,
        rx_status_led, rx_dmaready,
        tx_fifo_we, tx_fifo_wdata,
        tx_fifo_empty, tx_fifo_full,
        tx_fifo_afval, tx_fifo_aeval, tx_fifo_afull, tx_fifo_aempty,
        rx_fifo_re, rx_fifo_rdata,
        rx_fifo_empty, rx_fifo_full,
        rx_fifo_afval, rx_fifo_aeval, rx_fifo_afull, rx_fifo_aempty,
        interp, fcw, correct_i, correct_q,
        txen, txstop, ddsen, filteren,
        rxen, rxstop,
        duc_underrun, dac_last,
        ddc_overrun, adc_last):

    state_t = enum('IDLE', 'ACCESS', 'DONE',)
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
    
    @always_seq(pclk.posedge, reset=resetn)
    def controller():
        #pslverr.next = 0
        tx_dmaready.next = not tx_afull
        rx_dmaready.next = not rx_aempty

        tx_status_led.next = txen
        rx_status_led.next = rxen

        if clear_enable:
            txen.next = 0
            txstop.next = 0
            ddsen.next = 0
            filteren.next = 0
            overrun.next = 0

            rxen.next = 0
            rxstop.next = 0
            underrun.next = 0

        if not clear_ackn:
            clear_enable.next = False

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
            state.next = state_t.DONE
            if pwrite:
                if addr == WE_SAMPLE_ADDR:
                    if tx_fifo_full:
                        overrun.next = overrun + 1
                    else:
                        tx_fifo_wdata.next = pwdata
                        tx_fifo_we.next = True
                elif addr == WE_STATE_ADDR:
                    if pwdata[WES_CLEAR_BIT]:
                        clear_enable.next = True
                    elif pwdata[WES_TXSTOP_BIT]:
                        txstop.next = True
                    elif pwdata[WES_RXSTOP_BIT]:
                        rxstop.next = True
                    else:
                        txen.next = pwdata[WES_TXEN_BIT]
                        rxen.next = pwdata[WES_RXEN_BIT]
                        ddsen.next = pwdata[WES_DDSEN_BIT]
                        filteren.next = pwdata[WES_FILTEREN_BIT]
                elif addr == WE_INTERP_ADDR:
                    interp.next = pwdata
                elif addr == WE_FCW_ADDR:
                    fcw.next = pwdata
                elif addr == WE_THRESHOLD_ADDR:
                    afval.next = pwdata[28:16]
                    aeval.next = pwdata[12:]
                elif addr == WE_CORRECTION_ADDR:
                    correct_q.next = pwdata[26:16].signed()
                    correct_i.next = pwdata[10:].signed()
            else:
                if addr == WE_SAMPLE_ADDR:
                    if rx_fifo_empty:
                        underrun.next = underrun + 1
                    else:
                        rx_fifo_rdata.next = prdata
                        rx_fifo_re.next = True
                elif addr == WE_STATE_ADDR:
                    prdata.next = concat(
                        # BYTE 3 NIBBLE 2 - RX FIFO DATA/SPACE
                        intbv(0)[2:], not rx_fifo_empty, not rx_fifo_full,
                        # BYTE 3 NIBBLE 1 - RX FIFO AFULL/AEMPTY
                        intbv(0)[2:], rx_afull, rx_aempty,
                        # BYTE 2 NIBBLE 2 - FIFO DATA/SPACE
                        intbv(0)[2:], not tx_fifo_empty, not tx_fifo_full,
                        # BYTE 2 NIBBLE 1 - FIFO AFULL/AEMPTY
                        intbv(0)[2:], tx_afull, tx_aempty,
                        #intbv(0)[4:],
                        # BYTE 1 - DSP CHAIN
                        intbv(0)[5:], filteren, ddsen, txen,
                        #intbv(0)[8:],
                        # BYTE 0 - RESERVED
                        intbv(0)[8:])
                elif addr == WE_INTERP_ADDR:
                    prdata.next = interp
                elif addr == WE_FCW_ADDR:
                    prdata.next = fcw
                elif addr == WE_RUNS_ADDR:
                    prdata.next = concat(
                        rx_overrun[8:], underrun[8:],
                        tx_underrun[8:], overrun[8:])
                elif addr == WE_THRESHOLD_ADDR:
                    prdata.next = concat(intbv(0)[4:], afval,
                        intbv(0)[4:], aeval)
                elif addr == WE_CORRECTION_ADDR:
                    prdata.next = concat(intbv(0)[6:], correct_q[10:],
                            intbv(0)[6:], correct_i[10:])
        elif state == state_t.DONE:
            tx_fifo_afval.next = afval
            tx_fifo_aeval.next = aeval
            tx_fifo_we.next = False
            rx_fifo_afval.next = afval
            rx_fifo_aeval.next = aeval
            rx_fifo_re.next = False
            pready.next = True
            state.next = state_t.IDLE

    return synchronizer, controller
