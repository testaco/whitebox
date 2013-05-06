from myhdl import Signal, delay, always, always_comb, now, Simulation, intbv, instance, enum, traceSignals, concat

from simulate_utils import drive_system_clock, drive_reset
from apb3_utils import apb3_master_mock

#from util.genregs import parseregs

ADDR_MASK       = 0xff
TX_CONTROL_ADDR = 0x00
TX_ADDR         = 0x01
RX_CONTROL_ADDR = 0x02
RX_ADDR         = 0x03

def fluidsp_controller(pclk, presetn, paddr, psel, penable, pwrite, pwdata, pready, prdata, pslverr):
    tx_mode = Signal(intbv(0, 0, 2**30))
    tx_mutex = Signal(bool(0))
    tx_status = Signal(bool(0))

    rx_mode = Signal(intbv(0, 0, 2**30))
    rx_mutex = Signal(bool(0))
    rx_status = Signal(bool(0))

    def reset():
        print 'resetting'
        pready.next = 1
        pslverr.next = 0
        prdata.next = 0

    def write():
        if (paddr & ADDR_MASK) == TX_CONTROL_ADDR:
            print 'writing tx control register'
            tx_mode.next = pwdata[32:2]
            tx_mutex.next = pwdata[1] # TODO: Trying to write to this mutex when
                                      # it's already locked should result in an
                                      # error
        pready.next = 1

    def read():
        if (paddr & ADDR_MASK) == TX_CONTROL_ADDR:
            print 'reading tx control register'
            prdata.next = concat(tx_mode, tx_mutex, tx_status)
        pready.next = 1
    
    @always(pclk.posedge, presetn.negedge)
    def state_machine():
        if not presetn:
            reset()
        elif penable and psel and pwrite:
            write()
        elif psel and not pwrite:
            read()
        else:
            pready.next = 1
    
    return [state_machine, ]

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
    apb3_bus_signals = [system_clock, resetn, paddr, psel, penable, pwrite,
                        pwdata, pready, prdata, pslverr]

    SYSTEM_CLOCK_FREQ = 10e6
    SYSTEM_CLOCK_PERIOD_IN_NS = int(1.0 / SYSTEM_CLOCK_FREQ * 1e9)

    def testbench():
        clock = drive_system_clock(system_clock, SYSTEM_CLOCK_PERIOD_IN_NS)
        reset = drive_reset(resetn)
        master = apb3_master_mock([(0x40050400, 0xffffffff),
                                   (0x40050400, 0xffff7fff)],
                                  *apb3_bus_signals)
        slave = fluidsp_controller(*(apb3_bus_signals))
        return clock, reset, slave, master

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
    apb3_bus_signals = [system_clock, resetn, paddr, psel, penable, pwrite,
                        pwdata, pready, prdata, pslverr]
    toVerilog(fluidsp_controller, *(apb3_bus_signals))

FLUIDSP_CONTROLLER_REGS='''
########################################################################
## address 0
########################################################################
write_status    0[0]        0
write_mutex     0[1]        0
##reserved      0[2:31]     0

########################################################################
## address 1
########################################################################
read_status     1[0]        0
read_mutex      1[1]        0
##reserved      1[2:31]     0
'''

if __name__ == '__main__':
    #regs, mregs = parseregs(fulidsp_controller_regs)
    main_simulate()
    main_generate()
