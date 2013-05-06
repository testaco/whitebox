from myhdl import Signal, enum, intbv, always

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
