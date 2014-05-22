"""
FIFO
====

This is the fifo
"""
import random
import unittest

from myhdl import always, always_seq
from myhdl import delay
from myhdl import instance, instances
from myhdl import intbv
from myhdl import Signal, ResetSignal
from myhdl import StopSimulation
from myhdl import now
from myhdl import Simulation
from myhdl import traceSignals
from myhdl import enum
import myhdl

active_high = lambda s: s
active_low = lambda s: not s

posedge = lambda s: s.posedge
negedge = lambda s: s.negedge

def fifo(resetn,
         re,
         rclk,
         Q,
         we,
         wclk,
         data,
         full,
         afull,
         empty,
         aempty,
         afval,
         aeval,
         wack,
         dvld,
         overflow,
         underflow,
         rdcnt,
         wrcnt,
         **kwargs):
    """Main Fifo object.

    This only works as a simulation; when it is transpiled into Verilog,
    an auto-generated IP Core is used instead.  To see how to build the
    the IP core yourself with Libero IDE, check out this video (TODO).
    """

    width = kwargs['width']
    depth = kwargs['depth']
    write_active = kwargs.get('write_active', active_high)
    write_edge = kwargs.get('write_edge', posedge)
    read_active = kwargs.get('read_active', active_high)
    read_edge = kwargs.get('read_edge', posedge)
    reset_active = kwargs.get('reset_active', active_low)
    reset_edge = kwargs.get('reset_edge', negedge)
    threshold = kwargs.get('threshold', None)
    max_threshold = kwargs.get('max_threshold', None) 
    state_t = enum('IDLE', 'ACCESS',)
    rstate = Signal(state_t.IDLE)
    verbose = kwargs.get('verbose', False)

    _fifo = kwargs.get('load', [])

    Q_pipe1 = Signal(intbv(0)[len(Q):])
    dvld_pipe1 = Signal(bool(0))
    Q_pipe2 = Signal(intbv(0)[len(Q):])
    dvld_pipe2 = Signal(bool(0))

    @always_seq(write_edge(wclk), reset=resetn)
    def write():
        afull.next = len(_fifo) >= afval
        full.next = len(_fifo) >= depth
        if write_active(we):
            if len(_fifo) >= depth:
                if verbose:
                    print 'overflow'
                overflow.next = True
                wack.next = False
            else:
                if verbose:
                    print 'adding %d' % int(data)
                _fifo.insert(0, int(data))
                overflow.next = False
                wack.next = True
                wrcnt.next = wrcnt - 1
                rdcnt.next = rdcnt + 1
        else:
            overflow.next = False
            wack.next = False

    @always_seq(read_edge(rclk), reset=resetn)
    def read():
        aempty.next = len(_fifo) <= aeval
        empty.next = len(_fifo) == 0
        if read_active(re):
            if len(_fifo) == 0:
                if verbose:
                    print 'underflow'
                underflow.next = True
                dvld_pipe1.next = False
            else:
                if verbose:
                    print 'removing %d' % _fifo[-1]
                Q_pipe1.next = _fifo.pop()
                dvld_pipe1.next = True
                underflow.next = False
                wrcnt.next = wrcnt + 1
                rdcnt.next = rdcnt - 1
        else:
            underflow.next = False
            dvld_pipe1.next = False

        if dvld_pipe1:
            Q_pipe2.next = Q_pipe1
            dvld_pipe2.next = True
        else:
            dvld_pipe2.next = False

        if dvld_pipe2:
            Q.next = Q_pipe2
            dvld.next = True
        else:
            dvld.next = False


    return instances()

fifo.verilog_code = '''
actel_fifo_${width}_${depth} actel_fifo_${width}_${depth}_0(
    .DATA(${data}),
    .Q(${Q}),
    .WE(${we}),
    .RE(${re}),
    .WCLOCK(${wclk}),
    .RCLOCK(${rclk}),
    .FULL(${full}),
    .EMPTY(${empty}),
    .RESET(${resetn}),
    .AEMPTY(${aempty}),
    .AFULL(${afull}),
    .AEVAL(${aeval}),
    .AFVAL(${afval})
);
'''

class FifoSimulation(object):
    def __init__(self, depth=1024, width=32, duration=int(1e9/40e6)):
        self.rdata = []
        self.resetn = ResetSignal(0, 0, async=True)
        self.re = Signal(bool(0))
        self.rclk = Signal(bool(0))
        self.Q = Signal(intbv(0)[width:])
        self.we = Signal(bool(0))
        self.wclk = Signal(bool(0))
        self.data = Signal(intbv(0)[width:])
        self.full = Signal(bool(0))
        self.afull = Signal(bool(0))
        self.empty = Signal(bool(1))
        self.aempty = Signal(bool(1))
        self.afval = Signal(intbv(depth-1, min=0, max=depth))
        self.aeval = Signal(intbv(0, min=0, max=depth))
        self.wack = Signal(bool(0))
        self.dvld = Signal(bool(0))
        self.overflow = Signal(bool(0))
        self.underflow = Signal(bool(0))
        self.rdcnt = Signal(intbv(0, min=0, max=depth+1))
        self.wrcnt = Signal(intbv(depth, min=0, max=depth+1))
        self.duration = duration
        self.fifo = fifo(resetn=self.resetn,
                re=self.re,
                rclk=self.rclk,
                Q=self.Q,
                we=self.we,
                wclk=self.wclk,
                data=self.data,
                full=self.full,
                afull=self.afull,
                empty=self.empty,
                aempty=self.aempty,
                afval=self.afval,
                aeval=self.aeval,
                wack=self.wack,
                dvld=self.dvld,
                underflow=self.underflow,
                overflow=self.overflow,
                rdcnt=self.rdcnt,
                wrcnt=self.wrcnt,
                width=width,
                depth=depth)

    def reset(self):
        print '-- Reset --'
        self.resetn.next = False
        yield delay(2*self.duration)
        self.resetn.next = True

    def write(self, tx_data):
        print '-- Writing %s --' % hex(tx_data)
        print 'Write: start'
        self.data.next = intbv(tx_data)
        self.we.next = True #write_active(True)
        self.wclk.next = True
        yield delay(self.duration)

        print 'Write: stop'
        self.wclk.next = False
        yield delay(self.duration)
        self.we.next = False #write_active(False)

    def write_delay(self, tx_data):
        print 'Write: start'
        self.data.next = intbv(tx_data)
        self.wclk.next = True
        yield delay(self.duration)
        self.wclk.next = False
        yield delay(self.duration)
        print 'Write: stop'

    def read(self):
        print 'Read: start'
        self.re.next = True #write_active(True)
        self.rclk.next = True
        yield delay(self.duration)
        self.rclk.next = False
        yield delay(self.duration)
        self.re.next = False #write_active(False)

        #while not self.dvld and not self.underflow:
        #    self.rclk.next = True
        #    yield delay(self.duration)
        #    self.rclk.next = False
        #    yield delay(self.duration)
        #    
        print 'Read: stop'
        if self.dvld:
            self.rdata.append(int(self.Q))

    def read_delay(self):
        print 'Read: start'
        self.rclk.next = True
        yield delay(self.duration)
        self.rclk.next = False
        yield delay(self.duration)
        print 'Read: stop'
        if self.dvld:
            self.rdata.append(int(self.Q))
        
    def simulate(self, test):
        traced = traceSignals(test)
        s = Simulation(traced)
        s.run()

class TestFifo(unittest.TestCase):
    def test_fifo(self):
        depth = 4
        s = FifoSimulation(width=32, depth=depth)

        def test_fifo_interface():
            resetn = s.resetn
            re = s.re
            rclk = s.rclk
            Q = s.Q
            we = s.we
            wclk = s.wclk
            data = s.data
            full = s.full
            afull = s.afull
            empty = s.empty
            aempty = s.aempty
            afval = s.afval
            aeval = s.aeval
            wack = s.wack
            dvld = s.dvld
            overflow = s.overflow
            underflow = s.underflow
            rdcnt = s.rdcnt
            wrcnt = s.wrcnt

            @instance
            def stimulus():
                yield s.reset()
                afval.next = 3
                aeval.next = 1
                assert s.empty
                assert s.aempty
                assert not s.full
                assert not s.afull
                assert not s.wack
                assert not s.dvld
                assert not s.overflow
                assert not s.underflow
               
                for i in range(1, depth + 1):
                    if i <= afval + 1:
                        assert not s.afull
                    else:
                        assert s.afull
                    assert not s.full
                    assert s.wrcnt == depth - i + 1
                    assert s.rdcnt == i - 1
                    yield s.write(0xd0 + i)
                    assert s.wack
                    assert not s.overflow

                yield s.write_delay(0xd0 + depth + 1)
                assert s.afull
                assert s.full
                assert not s.wack
                assert not s.overflow

                yield s.write(0xd0 + depth + 1)
                assert s.afull
                assert s.full
                assert not s.wack
                assert s.overflow

                yield s.write_delay(0)
                assert s.afull
                assert s.full
                assert not s.wack
                assert not s.overflow

                for i in range(depth):
                    yield s.read()

                yield s.read_delay()
                yield s.read_delay()

                yield s.read()

                yield s.read_delay()
                yield s.read_delay()
                yield s.read_delay()

                print "Read", [hex(d) for d in s.rdata]

                assert len(s.rdata) == depth
                for i, d in enumerate(s.rdata):
                    assert 0xd0 + i + 1 == d

                raise StopSimulation
            
            return stimulus, s.fifo

        s.simulate(test_fifo_interface)

if __name__ == '__main__':
    unittest.main()
