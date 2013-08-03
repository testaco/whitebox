"""

The recevier works like so:
"""


from myhdl import Signal, delay, always, always_comb, always_seq, now, \
                  Simulation, intbv, instance, enum, traceSignals, \
                  toVerilog, StopSimulation, concat

from fifo import fifo as FIFO
from apb3_utils import Apb3Bus

class OverrunError(Exception):
    pass

class UnderrunError(Exception):
    pass

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
    depth=1024
    fifo = FIFO(*fifo_args,
        width=32,
        depth=depth,
        threshold=depth/2)

    ####### RESAMPLE ############
    decim = Signal(intbv(1)[32:])
    decim_counter = Signal(intbv(0)[32:])

    ########## STATE MACHINE ######
    state_t = enum('IDLE', 'READ_SAMPLE', 'DONE',)
    state = Signal(state_t.IDLE)

    rxen = Signal(bool(0))
    underrun = Signal(bool(False))
    overrun = Signal(bool(False))

    @always_seq(pclk.posedge, reset=resetn)
    def state_machine():
        if state == state_t.IDLE:
            if penable and psel:
                if paddr[8:] == 0x00 and not pwrite:
                    pready.next = 0
                    re.next = 1
                    state.next = state_t.READ_SAMPLE
                    if empty:
                        underrun.next = True

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
            prdata.next = Q
            re.next = 0
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
                    data.next = concat(adc_i_data[16:], adc_q_data[16:])
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

        decim = 4
        r = receiver(*signals)
        isig = functional_stimulus(sresetn, sclk, adc_i_data, lambda Q: Q+1 if Q < 1023 else 0)
        qsig = functional_stimulus(sresetn, sclk, adc_q_data, lambda Q: Q+1 if Q < 1023 else 0)

        scope = sample_scope(sresetn, sclk, bus.prdata)

        @instance
        def __sim():
            yield bus.reset()
            yield bus.receive(0x4001)
            assert bus.rdata & 0x0001 == 0
            yield bus.transmit(0x4002, decim) # decim rate
            yield bus.receive(0x4002)
            assert bus.rdata == decim
            yield bus.transmit(0x4001, 1)
            yield bus.receive(0x4001)
            assert bus.rdata & 0x0001 == 1
            yield bus.delay(40)
            for i in range(10000):
                if bus.rdata & 0x0010:
                    print '\n\n*************warning almost overrun'

                if bus.rdata & 0x0008:
                    print '\n\n*************warning almost underrun'
                    yield bus.delay(40)
                else:
                    for j in range(128):
                        yield bus.receive(0x4000)
                yield bus.receive(0x4001)
                if bus.rdata & 0x0002:
                    raise OverrunError
                if bus.rdata & 0x0004:
                    raise UnderrunError
            raise StopSimulation

        return __sim, r, isig, qsig, scope

    s = Simulation(stimulus, traceSignals(_sim))
    s.run()

    toVerilog(receiver, *signals)
