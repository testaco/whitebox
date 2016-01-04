from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal, toVerilog

from apb3_utils import Apb3Bus
from dsp import Signature

def modem(bus,
          clearn,
          dclk, bb_in, bb_out, data_in, data_out,
          **config):

    bb_out_valid = bb_out.valid
    bb_out_last = bb_out.last
    bb_out_i = bb_out.i
    bb_out_q = bb_out.q
    bb_in_valid = bb_in.valid
    bb_in_last = bb_in.last
    bb_in_i = bb_in.i
    bb_in_q = bb_in.q

    data_out_valid = data_out.valid
    data_out_last = data_out.last
    data_out_i = data_out.i
    data_out_q = data_out.q
    data_in_valid = data_in.valid
    data_in_last = data_in.last
    data_in_i = data_in.i
    data_in_q = data_in.q

    @always_seq(dclk.posedge, reset=clearn)
    def modulator():
        bb_out_valid.next = data_out_valid
        bb_out_last.next = data_out_last
        bb_out_i.next = data_out_i
        bb_out_q.next = data_out_q

    @always_seq(dclk.posedge, reset=clearn)
    def demodulator():
        data_in_valid.next = bb_in_valid
        data_in_last.next = bb_in_last
        data_in_i.next = bb_in_i
        data_in_q.next = bb_in_q

    return modulator, demodulator

def main():
    modem_config = {
    }

    bus = Apb3Bus()
    clearn = ResetSignal(0, 0, async=False)
    dclk = Signal(bool(0))
    bb_in  = Signature("bb_in", True, bits=16)
    bb_out = Signature("bb_out", True, bits=16)
    data_in  = Signature("data_in", True, bits=16)
    data_out = Signature("data_out", True, bits=16)
    signals = (
        bus,
        clearn,
        dclk, bb_in, bb_out, data_in, data_out,
    )

    toVerilog(modem, *signals, **modem_config)

if __name__ == '__main__':
    main()
