from myhdl import Signal, always, always_comb, always_seq, instances, \
                  intbv, enum, concat, modbv, ResetSignal, toVerilog

from apb3_utils import Apb3Bus
from ram import Ram#, Fifo
from dsp import Signature, iqmux, iqdemux

def streamer(bus,       # System bus
             clearn,    # Data clear
             dclk,      # Data clock (40MHz)
             tx_fifo_empty, tx_fifo_re, tx_fifo_dvld, tx_fifo_rdata,
             tx_fifo_underflow,
             data_out,
             rx_fifo_full, rx_fifo_we, rx_fifo_wdata,
             data_in,
             **config):
    
    #width = config.get('mem_width', 18)
    #depth = config.get('mem_depth', 1024)

    #ram = Ram(bus.presetn, bus.pclk, dclk, width=width, depth=depth)
    #ram_inst = ram.instance_type()(**ram.instance_signals())

    #fifo = Fifo(bus.presetn, ram.port['a'], ram.port['b'], width, depth) 
    #fifo_inst = fifo.instance_type()(**fifo.signals_dict())

    # CONTROLLER
    state_t = enum('IDLE', 'DONE',)
    state = Signal(state_t.IDLE)

    rx_overrun = Signal(bool(False))
    tx_underrun = Signal(bool(False))

    @always_seq(bus.pclk.posedge, reset=bus.presetn)
    def dma_controller():
        if state == state_t.IDLE:
            if bus.penable and bus.psel:
                bus.pready.next = False
                if bus.paddr[8:] == 0x00: # SAMPLE
                    state.next = state_t.DONE
                    if bus.pwrite:
                        # WRITE TO FIFO
                        pass
                    else:
                        # READ FROM FIFO
                        pass
                elif bus.paddr[8:] == 0x04: # Status
                    state.next = state_t.DONE
                    bus.prdata.next = concat(intbv(0)[30:], #
                                             tx_underrun, rx_overrun)
                else:
                    state.next = state_t.IDLE
        elif state == state_t.DONE:
            bus.pready.next = True
            state.next = state_t.IDLE

    @always_comb
    def assignments():
        data_out.valid.next = tx_fifo_dvld
        data_out.i.next = tx_fifo_rdata[15:]
        data_out.q.next = tx_fifo_rdata[31:16]

    txen = Signal(bool(0))
    txcnt = Signal(intbv(0, min=0, max=16))
    txinterp = Signal(intbv(1, min=1, max=16))

    @always_seq(dclk.posedge, reset=clearn)
    def dsp_controller():
        if data_in.valid:
            if rx_fifo_full:
                rx_overrun.next = True
            else:
                rx_fifo_we.next = True
                rx_fifo_wdata.next = concat(data_in.q, data_in.i)
        else:
            rx_fifo_we.next = False

        if txen:
            if txcnt == 0:
                txcnt.next = txinterp - 1
                if tx_fifo_empty:
                    tx_underrun.next = True
                else:
                    tx_fifo_re.next = True
            else:
                tx_fifo_re.next = False
                txcnt.next = txcnt - 1

    return instances()

def main():
    streamer_config = {
    }

    bus = Apb3Bus(duration=10)
    clearn = ResetSignal(0, 0, async=False)
    dclk = Signal(bool(0))
    tx_fifo_empty = Signal(bool(0))
    tx_fifo_re = Signal(bool(False))
    tx_fifo_dvld = Signal(bool(False))
    tx_fifo_rdata = Signal(intbv(0)[32:])
    tx_fifo_underflow = Signal(bool(False))
    data_out  = Signature("data_out", True, bits=16)

    rx_fifo_full = Signal(bool(False))
    rx_fifo_we = Signal(bool(False))
    rx_fifo_wdata = Signal(intbv(0)[32:])
    data_in = Signature("data_in", True, bits=16)
    signals = (
        bus,
        clearn,
        dclk,
        tx_fifo_empty, tx_fifo_re, tx_fifo_dvld, tx_fifo_rdata,
        tx_fifo_underflow,
        data_out,
        rx_fifo_full, rx_fifo_we, rx_fifo_wdata,
        data_in,
    )

    toVerilog(streamer, *signals, **streamer_config)

if __name__ == '__main__':
    main()
