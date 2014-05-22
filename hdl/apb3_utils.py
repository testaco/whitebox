"""

APB3 Bus Functional Model
=========================

A bus functional model is a way of using prodecural code to stimulate a HDL module, as though there were a real processor connected to it over the system bus.  You can execute read and write operations over the interface in a sequence, and assert the proper function.

This module implements the APB3 bus functional model, which is the common
peripheral model for the ARM Cortex series of processors.
It's not the fastest one, but it's more than capable of saturating the bandwidth
of the CMX991 (1MHz).
"""
from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        delay, now, Simulation, StopSimulation, traceSignals, \
        Cosimulation

apb3_bus_states = enum('IDLE', 'SETUP', 'ACCESS')

class Apb3TimeoutError(Exception):
    """Raised when a bus transaction times out."""
    pass

class Apb3Bus(object):
    def __init__(self, *args, **kwargs):
        """Initialize the bus.

        :param verbose: Add verbose logging
        :param duration: Clock period in ns
        :param timeout: Period to wait before timeout in ns
        """
        self.presetn = ResetSignal(0, 0, async=True)
        self.pclk = Signal(bool(0))
        self.paddr = Signal(intbv(0, 0, 2**32))
        self.psel = Signal(bool(0))
        self.penable = Signal(bool(0))
        self.pwrite = Signal(bool(1))
        self.pwdata = Signal(intbv(0, 0, 2**32))
        self.pready = Signal(bool(1))
        self.prdata = Signal(intbv(0, 0, 2**32))
        self.pslverr = Signal(bool(0))
        self.args = args
        self.kwargs = kwargs

    def debug(self, msg):
        if self.kwargs.get('verbose', False):
            print msg
        
    def reset(self):
        """Reset the bus."""
        duration = self.kwargs['duration']

        self.debug('-- Resetting --')
        self.presetn.next = True
        yield delay(duration)
        self.presetn.next = False
        yield self.delay(5)
        self.presetn.next = True

        self.debug('-- Reset --')

    def transmit(self, addr, data):
        """Transmit from master to slave.
        
        :param addr: The address to write to
        :param data: The data to write
        :raises Apb3TimeoutError: If slave doesn't set ``pready`` in time
        """
        assert not addr & 3  # Must be word aligned

        duration = self.kwargs['duration']
        timeout = self.kwargs.get('timeout') or 5 * duration

        self.debug('-- Transmitting addr=%s data=%s --' % (hex(addr), hex(data)))
        self.debug('TX: start')
        self.pclk.next = True
        self.paddr.next = intbv(addr)
        self.pwrite.next = True
        self.psel.next = True
        self.pwdata.next = intbv(data)
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

        self.debug('TX: enable')
        self.pclk.next = True
        self.penable.next = True
        yield delay(duration // 2)

        timeout_count = 0
        while not self.pready:
            self.debug('TX: wait')
            timeout_count += duration
            if timeout_count > timeout:
                raise Apb3TimeoutError
            self.pclk.next = False
            yield delay(duration // 2)
            self.pclk.next = True
            yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

        self.debug('TX: stop')
        self.pclk.next = True
        self.pwrite.next = False
        self.psel.next = False
        self.penable.next = False
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

    def receive(self, addr, assert_equals=None):
        """Receive from slave to master.
        
        :param addr: The address to read from
        :returns: Nothing, but sets ``self.rdata`` to the received data.
        :raises Apb3TimeoutError: If slave doesn't set ``pready`` in time
        """
        assert not addr & 3  # Must be word aligned
        duration = self.kwargs['duration']
        timeout = self.kwargs.get('timeout') or 5 * duration

        self.debug('-- Receiving addr=%s --' % (hex(addr),))
        self.debug('RX: start')
        self.pclk.next = True
        self.paddr.next = intbv(addr)
        self.pwrite.next = False
        self.psel.next = True
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

        self.debug('RX: enable')
        self.pclk.next = True
        self.penable.next = True
        yield delay(duration // 2)

        timeout_count = 0
        while not self.pready:
            self.debug('RX: wait')
            timeout_count += duration
            if timeout_count > timeout:
                raise Apb3TimeoutError
            self.pclk.next = False
            yield delay(duration // 2)
            self.pclk.next = True
            yield delay(duration // 2)

        self.pclk.next = False
        self.debug('RX: data=%s' % (hex(self.prdata),))
        self.rdata = self.prdata
        if assert_equals is not None:
            assert self.prdata == assert_equals, 'Got %s, expected %s' % (hex(self.prdata), hex(assert_equals))
        yield delay(duration // 2)

        self.debug('RX: stop')
        self.pclk.next = True
        self.pwrite.next = False
        self.psel.next = False
        self.penable.next = False
        yield delay(duration // 2)

        self.pclk.next = False
        yield delay(duration // 2)

    def dma_receive(self, ready, addr, count, **kwargs):
        """Receive multiple samples."""
        rdata = []
        for i in range(count):
            while not ready:
                yield self.delay(1)
            yield self.receive(addr)
            rdata.append(int(self.rdata))
            yield self.delay(2)
        self.rdata = rdata

    def delay(self, cycles):
        """Delay the bus a number of cycles."""
        duration = self.kwargs['duration']
        for i in xrange(cycles):
            self.pclk.next = True
            yield delay(duration // 2)
            self.pclk.next = False
            yield delay(duration // 2)

def apb3_simple_slave(bus, status_led):
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

    state_t = enum('IDLE', 'DONE',)
    state = Signal(state_t.IDLE)

    counter = Signal(intbv(0)[len(bus_prdata):])

    @always_seq(bus_pclk.posedge, reset=bus_presetn)
    def state_machine():
        status_led.next = counter[0]
        if state == state_t.IDLE:
            if bus_penable and bus_psel:
                if bus_paddr[8:] == 0x40:
                    bus_pready.next = False
                    if bus_pwrite:
                        counter.next = bus_pwdata
                        state.next = state_t.DONE
                    else:
                        bus_prdata.next = counter
                        counter.next = counter + 1
                        state.next = state_t.DONE
                else:
                    state.next = state_t.DONE
            else:
                state.next = state_t.IDLE
        elif state == state_t.DONE:
            bus_pready.next = True
            state.next = state_t.IDLE

    return state_machine

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

            status_led = Signal(bool(False))

            slave = apb3_simple_slave(bus, status_led)
            
            @myhdl.instance
            def __sim():
                yield bus.reset() 
                assert not status_led
                yield bus.transmit(0x40, 0x0001)
                assert status_led
                yield bus.receive(0x40)
                assert bus.rdata == 0x0001
                assert not status_led
                yield bus.receive(0x40)
                assert bus.rdata == 0x0002
                assert status_led
                raise StopSimulation
            return __sim, slave

        s = myhdl.Simulation(myhdl.traceSignals(_sim))
        s.run()

if __name__ == '__main__':
    unittest.main()
