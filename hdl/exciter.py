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

from myhdl import Signal, delay, always, always_comb, now, Simulation, intbv, instance, enum, traceSignals

import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

SYSTEM_CLOCK_FREQ = 10e6
SYSTEM_CLOCK_PERIOD_IN_NS = int(1.0 / SYSTEM_CLOCK_FREQ * 1e9)
print "SYSTEM_CLOCK_PERIOD_IN_NS: ", SYSTEM_CLOCK_PERIOD_IN_NS

def drive_system_clock(system_clock):
    period = SYSTEM_CLOCK_PERIOD_IN_NS
    low_time = int(period / 2)
    high_time = period - low_time

    @instance
    def drive_system_clock():
        while True:
            yield delay(low_time)
            system_clock.next = 1
            yield delay(high_time)
            system_clock.next = 0

    return drive_system_clock

apb3_bus_states = enum('IDLE', 'SETUP', 'ACCESS')

def apb3_master_mock(ops, pclk, presetn, paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr):
    apb3_fsm = Signal(apb3_bus_states.IDLE)
    written = Signal(bool(0))
    op_id = Signal(intbv(0, 0, len(ops)+1))

    @always(pclk.posedge, presetn.negedge)
    def state_machine():
        if not presetn:
            apb3_fsm.next = apb3_bus_states.IDLE
            print 'RESET'
            op_id.next = 0
            psel.next = 0
            penable.next = 0
        elif apb3_fsm == apb3_bus_states.IDLE:
            if op_id < len(ops):
                apb3_fsm.next = apb3_bus_states.SETUP
                psel.next = 1
                pwrite.next = 1#not written
                paddr.next = ops[op_id][0]
                pwdata.next = ops[op_id][1]
        elif apb3_fsm == apb3_bus_states.SETUP:
            print 'SETUP'
            apb3_fsm.next = apb3_bus_states.ACCESS
            penable.next = 1
        elif apb3_fsm == apb3_bus_states.ACCESS:
            if pready:
                print 'ACCESS'
                psel.next = 0
                penable.next = 0
                print "write", pwdata, "read", prdata
                apb3_fsm.next = apb3_bus_states.IDLE
                #written.next ue
                #pwrite.next = 0
                op_id.next = op_id + 1

    return state_machine

def exciter(pclk, presetn, paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr, dac_clock, dac_data):
    I = Signal(intbv(0, 0, 2**10))
    Q = Signal(intbv(0, 0, 2**10))
    I_const = Signal(intbv(0, 0, 2**10))
    Q_const = Signal(intbv(0, 0, 2**10))
    in_phase = Signal(bool(0))
    #register_file = [I_const, Q_const]

    @always(pclk.posedge, presetn.negedge)
    def state_machine():
        if not presetn:
            print 'resetting'
            pready.next = 1
            pslverr.next = 0
            prdata.next = 0;

            I_const.next = 0
            Q_const.next = 0
        elif penable and psel and pwrite:
            print 'paddr', paddr[8:0]
            if paddr[8:0] == 0:
                I_const.next = pwdata[16:6]
                Q_const.next = pwdata[32:22]
                pready.next = 1
            elif paddr[8:0] == 1:
                Q_const.next = pwdata[10:0]
                pready.next = 1
            else:
                pready.next = 1
                #pslverr.next = 1
        elif psel and not pwrite:
            #prdata.next = register_file[paddr]
            prdata.next = 0xbeef
            pready.next = 1
        else:
            pready.next = 1

    @always(pclk.posedge)
    def mode_machine():
        if presetn and not in_phase:  # Update the sample out of phase, locking
            I.next = I_const
            Q.next = Q_const

    @always(pclk.posedge)
    def dac_buffer():
        if presetn:
            dac_data.next = I[10:2] if in_phase else Q[10:2]
            dac_clock.next = not in_phase
            in_phase.next = not in_phase

    return state_machine, mode_machine, dac_buffer



def main_simulate():
    resetn = Signal(bool(1))
    system_clock = Signal(bool(0))
    paddr = Signal(intbv(0, 0, 2**32))
    psel = Signal(bool(0))
    penable = Signal(bool(0))
    pwrite = Signal(bool(1))
    pwdata = Signal(intbv(0, 0, 2**32))
    pready = Signal(bool(0))
    prdata = Signal(intbv(0, 0, 2**32))
    pslverr = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0, 0, 2**8))

    def testbench():
        apb3_bus_signals = [system_clock, resetn, paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr]
        clock = drive_system_clock(system_clock)
        master = apb3_master_mock([(0x40050400, 0xffffffff), (0x40050400, 0xffff7fff)],  # Set I=MAX, Q=0, should be CW
            *apb3_bus_signals)
        slave = exciter(*(apb3_bus_signals + [dac_clock, dac_data]))
        return clock, slave, master

    traced_testbench = traceSignals(testbench)
    sim = Simulation(traced_testbench)
    sim.run(SYSTEM_CLOCK_PERIOD_IN_NS * 100)

def main_generate():
    from myhdl import toVerilog
    resetn = Signal(bool(1))
    system_clock = Signal(bool(0))
    paddr = Signal(intbv(0, 0, 2**32))
    psel = Signal(bool(0))
    penable = Signal(bool(0))
    pwrite = Signal(bool(1))
    pwdata = Signal(intbv(0, 0, 2**32))
    pready = Signal(bool(0))
    prdata = Signal(intbv(0, 0, 2**32))
    pslverr = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0, 0, 2**8))
    apb3_bus_signals = [system_clock, resetn, paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr]
    toVerilog(exciter, *(apb3_bus_signals + [dac_clock, dac_data]))

if __name__ == '__main__':
    main_simulate()
    main_generate()
