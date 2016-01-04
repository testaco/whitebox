from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal, toVerilog, \
                  instances

from apb3_utils import Apb3Bus
from dsp import Signature

def converter(bus,
          clearn, dclk, sclk,
          rf_out,
          dac_en, dac_data,
          rf_in,
          adc_idata, adc_qdata,
          **config):

    rf_out_valid = rf_out.valid
    rf_out_last = rf_out.last
    rf_out_i = rf_out.i
    rf_out_q = rf_out.q
    rf_in_valid = rf_in.valid
    rf_in_last = rf_in.last
    rf_in_i = rf_in.i
    rf_in_q = rf_in.q

    @always_seq(dclk.posedge, reset=clearn)
    def dac_output():
        dac_en.next = False

    return instances()

def main():
    converter_config = {
    }

    bus = Apb3Bus()
    clearn = ResetSignal(0, 0, async=False)
    dclk = Signal(bool(0))
    sclk = Signal(bool(0))
    rf_out  = Signature("rf_out", True, bits=16)
    rf_in = Signature("rf_in", True, bits=16)
    dac_data = Signal(intbv(0)[10:])
    dac_en = Signal(bool(0))
    adc_idata = Signal(intbv(0, min=-2**9, max=2**9))
    adc_qdata = Signal(intbv(0, min=-2**9, max=2**9))
    signals = (
        bus,
        clearn,
        dclk, bb_in, bb_out, data_in, data_out,
    )

    toVerilog(converter, *signals, **converter_config)

if __name__ == '__main__':
    main()
