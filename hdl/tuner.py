from myhdl import Signal, always, always_comb, always_seq, instances, \
                  intbv, enum, concat, modbv, ResetSignal, toVerilog

from apb3_utils import Apb3Bus
from dsp import Signature, iqmux, iqdemux
from dds import dds, freq_to_fcw
from cic import cic2


def sync(inclk, insig, clearn, outclk, outsig):
    inclk1 = Signal(bool(False))

    @always_seq(outclk.posedge, reset=clearn)
    def synchronize():
        inclk1.next = inclk
        if inclk and not inclk1: # Posedge
            outsig.valid.next = insig.valid
            outsig.last.next = insig.last
            outsig.i.next = insig.i
            outsig.q.next = insig.q
        else:
            outsig.valid.next = False
            outsig.last.next = False
            outsig.i.next = 0
            outsig.q.next = 0

    return instances()


def resampler(clearn, dclk, insig, outsig, rate, mode):
    i = Signal(intbv(0)[len(rate):])

    @always_seq(dclk.posedge, reset=clearn)
    def resample():
        if insig.valid:
            if i == 0:
                outsig.valid.next = True
                outsig.last.next = insig.last
                outsig.i.next = insig.i
                outsig.q.next = insig.q
                i.next = rate - 1
            else:
                outsig.valid.next = False
                outsig.last.next = False
                outsig.i.next = 0
                outsig.q.next = 0
                i.next = i - 1
        else:
            outsig.valid.next = False
            outsig.last.next = False
            outsig.i.next = 0
            outsig.q.next = 0

    return resample


def qmixer(clearn, dclk, insig, losig, outsig):
    insig1 = insig.copy('insig1')
    state_t = enum('IDLE', 'PROCESSING1', 'PROCESSING2')
    state = Signal(state_t.IDLE)

    ia = Signal(intbv(0, min=-2**(len(insig.i) + len(losig.i) - 1),
                         max=2**(len(insig.i) + len(losig.i) - 1)))
    ib = Signal(intbv(0, min=-2**(len(insig.i) + len(losig.i) - 1),
                         max=2**(len(insig.i) + len(losig.i) - 1)))
    qa = Signal(intbv(0, min=-2**(len(insig.q) + len(losig.q) - 1),
                         max=2**(len(insig.q) + len(losig.q) - 1)))
    qb = Signal(intbv(0, min=-2**(len(insig.q) + len(losig.q) - 1),
                         max=2**(len(insig.q) + len(losig.q) - 1)))

    i = Signal(intbv(0, min=-2**(len(insig.i) + len(outsig.i) - 1),
                        max=2**(len(insig.i) + len(outsig.i) - 1)))
    q = Signal(intbv(0, min=-2**(len(insig.q) + len(outsig.q) - 1),
                        max=2**(len(insig.q) + len(outsig.q) - 1)))

    @always_comb
    def assignments():
        insig.ready.next = (state == state_t.IDLE)
        losig.ready.next = insig.valid

        outsig.i.next = i[:len(i)-len(outsig.i)]
        outsig.q.next = q[:len(q)-len(outsig.q)]

    @always_seq(dclk.posedge, reset=clearn)
    def qmix():
        if state == state_t.IDLE:
            outsig.valid.next = False
            if insig.valid:
                state.next = state_t.PROCESSING1
                # Delay the in signal to processing step
                insig1.i.next = insig.i
                insig1.q.next = insig.q
            else:
                state.next = state_t.IDLE
                insig1.i.next = 0
                insig1.q.next = 0
        elif state == state_t.PROCESSING1:
            ia.next = insig1.i.signed() * losig.i.signed()
            ib.next = insig1.q.signed() * losig.q.signed()
            qa.next = insig1.i.signed() * losig.q.signed()
            qb.next = insig1.q.signed() * losig.i.signed()
            state.next = state_t.PROCESSING2
        elif state == state_t.PROCESSING2:
            outsig.valid.next = True
            i.next = ia.signed() - ib.signed()
            q.next = qa.signed() + qb.signed()
            state.next = state_t.IDLE

    return instances()


def tuner(bus,       # System bus
          clearn,    # Data clear
          dclk,      # Data clock (40MHz)
          bb_in,     # Baseband transmit signal
          bb_out,    # Baseband receive signal
          rf_in,     # RX RF signal
          rf_out,    # TX RF signal
          **config):

    assert rf_in.rangetuple() == rf_out.rangetuple() == \
           bb_in.rangetuple() == bb_out.rangetuple()

    # Makes sure all signals are named by the top-level
    bb_in_valid = bb_in.valid
    bb_in_last = bb_in.last
    bb_in_i = bb_in.i
    bb_in_q = bb_in.q
    bb_out_valid = bb_out.valid
    bb_out_last = bb_out.last
    bb_out_i = bb_out.i
    bb_out_q = bb_out.q
    rf_in_ready = rf_in.ready
    rf_in_valid = rf_in.valid
    rf_in_last = rf_in.last
    rf_in_i = rf_in.i
    rf_in_q = rf_in.q
    rf_out_valid = rf_out.valid
    rf_out_last = rf_out.last
    rf_out_i = rf_out.i
    rf_out_q = rf_out.q

    # Tuning parameters

    # Transmit / Receive
    trx = Signal(bool(0))    # 0 = receive, 1 = transmit
    # DDS Control Switch
    ddsen = Signal(bool(0))  # 0 = dds off, 1 = dds on
    fcw_bitwidth = 25
    # DDS frequency control word
    fcw = Signal(intbv(
        freq_to_fcw(0, sample_rate=10e6,
                    phase_accumulator_bitwidth=fcw_bitwidth),
        min=0, max=2**fcw_bitwidth))
    # CIC interpolation factor
    cic_interp = Signal(intbv(1, min=1, max=50))
    # CIC shift factor
    cic_shift = Signal(intbv(0, min=0, max=10))

    # Bus Slave
    @always_seq(bus.pclk.posedge, reset=bus.presetn)
    def slave():
        bus.pready.next = True
        if bus.psel and bus.penable:
            if bus.paddr == 0: # DDSEN, TRX
                if bus.pwrite:
                    trx.next = bus.pwdata[0]
                    ddsen.next = bus.pwdata[1]
                else:
                    bus.prdata.next = concat(intbv(0)[30:], ddsen, trx)
            elif bus.paddr == 1: # FCW
                if bus.pwrite:
                    fcw.next = bus.pwdata[fcw_bitwidth:]
                else:
                    bus.prdata.next = concat(
                        intbv(0)[len(bus.prdata)-fcw_bitwidth:], fcw)
            elif bus.paddr == 2: # CIC_INTERP
                if bus.pwrite:
                    cic_interp.next = bus.pwdata[len(cic_interp):]
                else:
                    bus.prdata.next = concat(
                        intbv(0)[len(bus.prdata)-len(cic_interp):], cic_interp)
            elif bus.paddr == 3: # CIC_SHIFT
                if bus.pwrite:
                    cic_shift.next = bus.pwdata[len(cic_shift):]
                if bus.pwrite:
                    bus.prdata.next = concat(
                        intbv(0)[len(bus.prdata)-len(cic_shift):], cic_shift)
        else:
            bus.prdata.next = 0

    @always_seq(dclk.posedge, reset=clearn)
    def controller():
        rf_in_ready.next = True

    # Signals
    rfd_in = rf_in.copy('rfd_in') # RF signal synced to dclk
    lo = rf_in.copy('lo')         # Output of DDS
    rx_sig = rf_in.copy('rx_sig') # Between mixer and CIC
    tx_sig = rf_in.copy('tx_sig') # Between CIC and mixer
    cic_in = rf_in.copy('cic_in')
    cic_out = rf_in.copy('cic_out')
    mix_in = rf_in.copy('mix_in')
    mix_out = rf_in.copy('mix_out')

    # Blocks
    cic0 = resampler(clearn, dclk, cic_in, cic_out, cic_interp, trx)
    qmix0 = qmixer(clearn, dclk, mix_in, lo, mix_out)
    dds0 = dds(clearn, dclk, ddsen, lo, fcw)

    # Interconnect
    cic_in_mux = iqmux(clearn, dclk, trx, rx_sig, bb_out, cic_in)
    cic_out_demux = iqdemux(clearn, dclk, trx, cic_out, bb_in, tx_sig)
    mix_in_mux = iqmux(clearn, dclk, trx, rf_in, tx_sig, mix_in)
    mix_out_demux = iqdemux(clearn, dclk, trx, mix_out, rx_sig, rf_out)

    return instances()

def main():
    tuner_config = {
    }

    bus = Apb3Bus(duration=10)
    clearn = ResetSignal(0, 0, async=False)
    dclk = Signal(bool(0))
    bb_in  = Signature("bb_in", True, bits=16)
    bb_out = Signature("bb_out", True, bits=16)
    rf_in  = Signature("rf_in", True, bits=16)
    rf_out = Signature("rf_out", True, bits=16)
    signals = (
        bus,
        clearn,
        dclk, bb_in, bb_out,
        rf_in, rf_out,
    )

    toVerilog(tuner, *signals, **tuner_config)

if __name__ == '__main__':
    main()
