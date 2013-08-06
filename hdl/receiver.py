"""

The recevier works like so:
"""


from myhdl import Signal, delay, always, always_comb, always_seq, now, \
                  Simulation, intbv, instance, enum, traceSignals, \
                  toVerilog, StopSimulation, concat, now

from fifo import fifo as FIFO
from dds import dds as DDS
from apb3_utils import Apb3Bus

class OverrunError(Exception):
    pass

class UnderrunError(Exception):
    pass

BULK_SIZE=64
DECIM=200
FCW=int(40e6)
SAMPLES_TO_SIMULATE=1000

def receiver(
        resetn,
        system_clock,
        pclk,
        paddr,
        psel,
        penable,
        pwrite,
        pwdata,
        pready,
        prdata,
        pslverr,
        adc_clock,
        adc_i_data,
        adc_q_data):
    ####### FIFO ############
    # Read
    re = Signal(bool(False))
    rclk = pclk
    Q = Signal(intbv(0)[32:])

    # Write
    we = Signal(bool(False))
    wclk = system_clock
    data = Signal(intbv(0)[32:])

    # Threshold
    full = Signal(bool(False))
    full.driven = 'wire'
    afull = Signal(bool(False))
    afull.driven = 'wire'
    empty = Signal(bool(False))
    empty.driven = 'wire'
    aempty = Signal(bool(False))
    aempty.driven = 'wire'

    fifo_args = resetn, re, rclk, Q, we, wclk, data, full, afull, \
        empty, aempty
    fifo = FIFO(*fifo_args,
        width=32,
        depth=BULK_SIZE*4,
        threshold=BULK_SIZE*1)

    ####### RESAMPLE ############
    decim = Signal(intbv(1)[32:])
    decim_counter = Signal(intbv(0)[32:])

    ########## STATE MACHINE ######
    state_t = enum('IDLE', 'READ_SAMPLE', 'READ_SAMPLE2', 'DONE',)
    state = Signal(state_t.IDLE)

    rxen = Signal(bool(0))
    underrun = Signal(bool(False))
    overrun = Signal(bool(False))

    @always_seq(pclk.posedge, reset=resetn)
    def state_machine():
        if state == state_t.IDLE:
            if penable and psel:
                if paddr[8:] == 0x00 and not pwrite:
                    if empty:
                        underrun.next = True
                        state.next = state_t.DONE
                    else:
                        pready.next = 0
                        re.next = 1
                        state.next = state_t.READ_SAMPLE

                elif paddr[8:] == 0x01:
                    if pwrite:
                        rxen.next = pwdata[0]
                    else:
                        prdata.next = concat(afull, aempty, underrun, overrun, rxen)
                    state.next = state_t.DONE

                elif paddr[8:] == 0x02:
                    if pwrite:
                        decim.next = pwdata
                    else:
                        prdata.next = decim
                    state.next = state_t.DONE

        elif state == state_t.READ_SAMPLE:
            re.next = 0
            state.next = state_t.READ_SAMPLE2

        elif state == state_t.READ_SAMPLE2:
            prdata.next = Q
            state.next = state_t.DONE
            
        elif state == state_t.DONE:
            pready.next = 1
            state.next = state_t.IDLE

    @always(system_clock.posedge)
    def sampler():
        if rxen:
            adc_clock.next = not adc_clock
            if adc_clock:
                decim_counter.next = decim_counter + 1
                if decim_counter == decim:
                    decim_counter.next = 0
                    data.next = concat(adc_i_data[10:], intbv(0)[6:], adc_q_data[10:], intbv(0)[6:])
                    we.next = True
            if full:
                overrun.next = True
            if we:
                we.next = False


    return fifo, state_machine, sampler

def functional_stimulus(resetn, sclk, Q, l):
    @always_seq(sclk.posedge, reset=resetn)
    def state_machine():
        Q.next = l(Q)

    return state_machine

def sample_scope(resetn, sclk, sample):
    i = Signal(intbv(0)[32:])
    q = Signal(intbv(0)[32:])

    @always_seq(sclk.posedge, reset=resetn)
    def state_machine():
        i.next = sample[32:17]
        q.next = sample[16:]

    return state_machine

if __name__ == '__main__':
    APB3_DURATION = int(1e9 / 10e6)
    SYSCLK_DURATION = int(1e9 / 20e6)
    bus = Apb3Bus(duration=APB3_DURATION)
    
    sclk = Signal(bool(0))
    adc_clock = Signal(bool(0))
    adc_i_data = Signal(intbv(0)[10:])
    adc_q_data = Signal(intbv(0)[10:])

    signals = (bus.presetn,
                sclk,
                bus.pclk,
                bus.paddr,
                bus.psel,
                bus.penable,
                bus.pwrite,
                bus.pwdata,
                bus.pready,
                bus.prdata,
                bus.pslverr,
                adc_clock,
                adc_i_data,
                adc_q_data)

    @always(delay(SYSCLK_DURATION // 2))
    def stimulus():
        sclk.next = not sclk

    def _sim():
        sresetn = bus.presetn
        bus_pclk = bus.pclk
        bus_paddr = bus.paddr
        bus_psel = bus.psel
        bus_penable = bus.penable
        bus_pwrite = bus.pwrite
        bus_pwdata = bus.pwdata
        bus_pready = bus.pready
        bus_prdata = bus.prdata
        bus_pslverr = bus.pslverr
        i_sample = Signal(bus.prdata[32:17])
        q_sample = Signal(bus.prdata[16:])
        N = Signal(intbv(0)[32:])

        decim = DECIM
        r = receiver(*signals)
        fcw = Signal(intbv(0)[32:])
        isig = DDS(sresetn, sclk, adc_i_data, fcw)
        qsig = DDS(sresetn, sclk, adc_q_data, fcw)

        scope = sample_scope(sresetn, sclk, bus.prdata)
        samples = []

        @instance
        def __sim():
            fcw.next = FCW
            yield bus.reset()
            yield bus.receive(0x4001)
            assert bus.rdata & 0x0001 == 0
            yield bus.transmit(0x4002, decim) # decim rate
            yield bus.receive(0x4002)
            assert bus.rdata == decim
            yield bus.transmit(0x4001, 1)
            yield bus.receive(0x4001)
            assert bus.rdata & 0x0001 == 1

            # Wait for the buffer to pass being underrun
            t0 = now()

            while N < SAMPLES_TO_SIMULATE:
                yield bus.receive(0x4001)
                while bus.rdata & 0x0008:
                    yield bus.delay(BULK_SIZE*100)
                    yield bus.receive(0x4001)

                if bus.rdata & 0x0002:
                    raise OverrunError
                if bus.rdata & 0x0004:
                    raise UnderrunError
                if bus.rdata & 0x0010:
                    print '\n\n*************warning almost overrun'

                for j in range(BULK_SIZE):
                    yield bus.receive(0x4000)
                    samples.append(int(bus.rdata[32:17]))
                    N.next = N + 1

                yield bus.delay(256) # Simulate processing

            import json
            print json.dumps(samples)
            print "Processed %d samples in %d ns." % (N, now() - t0)
            raise StopSimulation

        return __sim, r, isig, qsig, scope

    s = Simulation(stimulus, traceSignals(_sim))
    s.run()

    toVerilog(receiver, *signals)
