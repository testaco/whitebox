from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

def ddc_fake(clearn, dac_clock,
            fifo_full, fifo_we, fifo_wdata,
            rxen, rxstop, filteren, decim, correct_i, correct_q,
            overrun, sample,
            adc_idata, adc_qdata, adc_last, **kwargs):

    sample_valid = sample.valid
    sample_last = sample.last
    sample_i = sample.i
    sample_q = sample.q

    @always_seq(dac_clock.posedge, reset=clearn)
    def pass_through():
        overrun.next = 0
        fifo_we.next = 0

    return pass_through

def ddc(clearn, dac_clock,
            fifo_full, fifo_we, fifo_wdata,
            rxen, rxstop, filteren, decim, correct_i, correct_q,
            overrun, sample,
            adc_idata, adc_qdata, adc_last, **kwargs):
    pass
