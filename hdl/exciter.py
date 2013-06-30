"""

The exciter works like so:
    1. Sensors, like the 1-bit key, or the 10-bit adc from microphone
       even Memory!  Perhaps always memory?  It always comes at a bitrate.
    2. Encoders, that take data and convert them into symbols at a baud rate.
    3. Right now, everything is quadrature data, I[n] & Q[n].  These are 16 bit fixed
       point numbers.
    4. The mode outputs, always I[n] & Q[n] are fed to the DAC, which has analog errors.
    5. A filter is applied to the outputted I[n] & Q[n] analog signals.
    6. The analog radio:
        1. Quadrature Modulates the signal with the carrier f_if
            I[n]*cos(2*pi*f_if) + Q[n]*sin(2*pi*f_if)
        2. It then filters the output
        3. It then Image-Reject upconverts with the f_rf
    7. The frontend then:
        1. goes through a matching balun network
        2. into a filter
        3. then an amplifier
        4. then an antenna

"""

from myhdl import Signal, delay, always, always_comb, always_seq, now, \
                  Simulation, intbv, instance, enum, traceSignals, \
                  toVerilog, StopSimulation

from fifo import fifo as FIFO
from apb3_utils import Apb3Bus

APB3_DURATION = int(1e9 / 10e6)
SYSCLK_DURATION = int(1e9 / 20e6)

class OverrunError(Exception):
    pass

def exciter(
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
        dac_clock,
        dac_data):

    ####### FIFO ############
    # Read
    re = Signal(bool(False))
    rclk = system_clock
    Q = Signal(intbv(0)[32:])

    # Write
    we = Signal(bool(False))
    wclk = pclk
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
        depth=1024)


    ######### RESAMPLER ###########

    ######### INTERLEAVER #########
    in_phase = Signal(bool(0))
    sample_i = Signal(intbv(0, 0, 2**10))
    sample_q = Signal(intbv(0, 0, 2**10))

    ########## STATE MACHINE ######
    state_t = enum('IDLE', 'WRITE_SAMPLE', 'DONE',)
    state = Signal(state_t.IDLE)

    ############ TX EN ###########
    txen = Signal(bool(0))

    @always_seq(pclk.posedge, reset=resetn)
    def state_machine():
        if state == state_t.IDLE:
            if penable and psel and pwrite:
                if paddr[8:] == 0x00:
                    state.next = state_t.WRITE_SAMPLE
                    pready.next = 0
                    we.next = 1
                    data.next = pwdata
                    if full:
                        raise OverrunError
                elif paddr[8:] == 0x01:
                    print 'hi', pwdata
                    txen.next = pwdata[0]
            elif psel and not pwrite:
                pass

        elif state == state_t.WRITE_SAMPLE:
            we.next = 0
            state.next = state_t.DONE
            
        elif state == state_t.DONE:
            pready.next = 1
            state.next = state_t.IDLE

    @always(system_clock.posedge)
    def resampler():
        if txen:  # Update the sample out of phase, locking
            if re:
                sample_i.next = Q[9:]
                sample_q.next = Q[32:23]
            re.next = not re

    @always(system_clock.posedge)
    def interleaver():
        if txen:
            dac_data.next = sample_i[10:2] if in_phase else sample_q[10:2]
            dac_clock.next = not in_phase
            in_phase.next = not in_phase

    return fifo, state_machine, resampler, interleaver


if __name__ == '__main__':
    bus = Apb3Bus(duration=APB3_DURATION)
    
    sclk = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0)[10:])

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
                dac_clock,
                dac_data)

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

        e = exciter(*signals)

        @instance
        def __sim():
            yield bus.reset()
            for i in range(8):
                yield bus.transmit(0x4000, i)
            yield bus.transmit(0x4001, 0x01)
            yield delay(APB3_DURATION*6)
            raise StopSimulation


        return __sim, e

    s = Simulation(stimulus, traceSignals(_sim))
    s.run()

    toVerilog(exciter, *signals)
