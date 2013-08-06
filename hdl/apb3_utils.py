from myhdl import ResetSignal, Signal, enum, intbv, always, delay

apb3_bus_states = enum('IDLE', 'SETUP', 'ACCESS')

class Apb3TimeoutError(Exception):
    pass

class Apb3Bus(object):
    def __init__(self, *args, **kwargs):
        #self.presetn = Signal(bool(1))
        self.presetn = ResetSignal(0, 0, async=True)
        self.pclk = Signal(bool(0))
        self.paddr = Signal(intbv(0, 0, 2**32))
        self.psel = Signal(bool(0))
        self.penable = Signal(bool(0))
        self.pwrite = Signal(bool(1))
        self.pwdata = Signal(intbv(0, 0, 2**32))
        self.pready = Signal(bool(0))
        self.prdata = Signal(intbv(0, 0, 2**32))
        self.pslverr = Signal(bool(0))
        self.args = args
        self.kwargs = kwargs
        
    def reset(self):
        duration = self.kwargs['duration']

        print '-- Resetting --'
        self.presetn.next = True
        yield delay(duration)
        self.presetn.next = False
        yield delay(duration * 5)
        self.presetn.next = True

        print '-- Reset --'
        yield delay(duration * 5)

    def transmit(self, addr, data):
        duration = self.kwargs['duration']
        timeout = self.kwargs.get('timeout') or 5 * duration

        print '-- Transmitting addr=%s data=%s --' % (hex(addr), hex(data))
        print 'TX: start'
        self.pclk.next = True
        self.paddr.next = intbv(addr)
        self.pwrite.next = True
        self.psel.next = True
        self.pwdata.next = intbv(data)
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

        print 'TX: enable'
        self.pclk.next = True
        self.penable.next = True
        yield delay(duration // 2)

        timeout_count = 0
        while not self.pready:
            print 'TX: wait'
            timeout_count += duration
            if timeout_count > timeout:
                raise Apb3TimeoutError
            self.pclk.next = False
            yield delay(duration // 2)
            self.pclk.next = True
            yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

        print 'TX: stop'
        self.pclk.next = True
        self.pwrite.next = False
        self.psel.next = False
        self.penable.next = False
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

    def receive(self, addr, assert_equals=None):
        duration = self.kwargs['duration']
        timeout = self.kwargs.get('timeout') or 5 * duration

        print '-- Receiving addr=%s --' % (hex(addr),)
        print 'RX: start'
        self.pclk.next = True
        self.paddr.next = intbv(addr)
        self.pwrite.next = False
        self.psel.next = True
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

        print 'RX: enable'
        self.pclk.next = True
        self.penable.next = True
        yield delay(duration // 2)

        timeout_count = 0
        while not self.pready:
            print 'RX: wait'
            timeout_count += duration
            if timeout_count > timeout:
                raise Apb3TimeoutError
            self.pclk.next = False
            yield delay(duration // 2)
            self.pclk.next = True
            yield delay(duration // 2)

        self.pclk.next = False
        print 'RX: data=%s' % (hex(self.prdata),)
        self.rdata = self.prdata
        if assert_equals is not None:
            assert self.prdata == assert_equals, 'Got %s, expected %s' % (hex(self.prdata), hex(assert_equals))
        yield delay(duration // 2)

        print 'RX: stop'
        self.pclk.next = True
        self.pwrite.next = False
        self.psel.next = False
        self.penable.next = False
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

    def delay(self, cycles):
        duration = self.kwargs['duration']
        for i in xrange(cycles):
            self.pclk.next = True
            yield delay(duration // 2)
            self.pclk.next = False
            yield delay(duration // 2)


import unittest

class TestApb3BusFunctionalModel(unittest.TestCase):
    def test_simulate(self):
        import myhdl
        duration=1

        def _sim():
            bus = Apb3Bus(duration=duration)
            bus_presetn = bus.presetn
            bus_pclk = bus.pclk
            bus_paddr = bus.paddr
            bus_psel = bus.psel
            bus_penable = bus.penable
            bus_pwrite = bus.pwrite
            bus_pwdata = bus.pwdata
            bus_pready = bus.pready
            bus_prdata = bus.prdata
            bus_pslverr = bus.pslverr
            
            @myhdl.instance
            def __sim():
                yield bus.reset() 
                yield bus.transmit(0x4000, 0x0110)
                yield bus.receive(0x4000)
            return __sim

        s = myhdl.Simulation(myhdl.traceSignals(_sim))
        s.run(10000)

if __name__ == '__main__':
    unittest.main()
