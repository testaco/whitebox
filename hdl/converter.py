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
    """dclk >>> sclk (at least 4x)"""

    rf_out_valid = rf_out.valid
    rf_out_last = rf_out.last
    rf_out_i = rf_out.i
    rf_out_q = rf_out.q
    rf_in_valid = rf_in.valid
    rf_in_last = rf_in.last
    rf_in_i = rf_in.i
    rf_in_q = rf_in.q

    rxen = Signal(bool(0))
    rxen1 = Signal(bool(0))
    rxen2 = Signal(bool(0))
    txen = Signal(bool(0))
    txen1 = Signal(bool(0))
    txen2 = Signal(bool(0))

    sclk1 = Signal(bool(0))
    sclk2 = Signal(bool(0))

    adc_bitlength = len(adc_idata)
    adc_idata1 = Signal(intbv(0, min=-2**(adc_bitlength-1), max=2**(adc_bitlength-1)))
    adc_idata2 = Signal(intbv(0, min=-2**(adc_bitlength-1), max=2**(adc_bitlength-1)))
    adc_qdata1 = Signal(intbv(0, min=-2**(adc_bitlength-1), max=2**(adc_bitlength-1)))
    adc_qdata2 = Signal(intbv(0, min=-2**(adc_bitlength-1), max=2**(adc_bitlength-1)))

    @always_seq(bus.pclk.posedge, reset=bus.presetn)
    def controller():
        bus.pready.next = True
        if bus.psel and bus.penable:
            if bus.paddr == 0x00:
                if bus.pwrite:
                    rxen.next = bus.pwdata[0]
                    txen.next = bus.pwdata[1]
                else:
                    bus.prdata.next = concat(intbv(0)[30:], txen, rxen)

    @always_seq(dclk.posedge, reset=clearn)
    def sampling_clock_synchronizer():
        # Two delay sync from sampling clock to dsp clock
        sclk1.next = sclk
        sclk2.next = sclk1

    @always_seq(dclk.posedge, reset=clearn)
    def adc_input():
        # Two delay sync from soc clock to dsp clock
        rxen1.next = rxen
        rxen2.next = rxen1

        adc_idata1.next = adc_idata
        adc_idata2.next = adc_idata1
        adc_qdata1.next = adc_qdata
        adc_qdata2.next = adc_qdata1

        # Posedge detect to signal streaming bus
        if rxen2 and sclk1 and not sclk2:
            rf_in_valid.next = True
            rf_in_i.next = concat(adc_idata2[adc_bitlength-1], adc_idata2[adc_bitlength-1], adc_idata2[adc_bitlength-1],
                                  adc_idata2[adc_bitlength-1], adc_idata2[adc_bitlength-1], adc_idata2[adc_bitlength-1], adc_idata2)
            rf_in_q.next = concat(adc_qdata2[adc_bitlength-1], adc_qdata2[adc_bitlength-1], adc_qdata2[adc_bitlength-1],
                                  adc_qdata2[adc_bitlength-1], adc_qdata2[adc_bitlength-1], adc_qdata2[adc_bitlength-1], adc_qdata2)
        else:
            rf_in_valid.next = False
            rf_in_i.next = 0
            rf_in_q.next = 0

    @always_seq(dclk.posedge, reset=clearn)
    def dac_output():
        # Two delay sync from soc clock to dsp clock
        txen1.next = txen
        txen2.next = txen1
        # Negedge detect starts reading sample in 
        if txen2 and sclk2 and not sclk1:
            rf_out.ready.next = True
        else:
            rf_out.ready.next = False

        # On the next dsp clock step, rf_out.valid is true
        if rf_out.valid:
            dac_en.next = not rf_out.last
            dac_data.next = rf_out.q
    #        rf_out_i.next = rf_out.i
    #        rf_out_valid.next = rf_out.valid

    #    #  on the next dac_clock switch from high to low
    #    if rf_out_valid and sclk2 and not sclk1:
    #        # Put out I1 on dac_data
    #        dac_data.next = rf_out2.clk

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
