"""
FIFO
====

This is the fifo
"""
import random

from myhdl import always
from myhdl import delay
from myhdl import instance, instances
from myhdl import intbv
from myhdl import Signal
from myhdl import StopSimulation
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
    _fifo = []

    size = Signal(intbv(0)[32:])

    @always(reset_edge(resetn))
    def reset():
        _fifo = []
        full.next = False
        afull.next = False
        empty.next = True
        aempty.next = True

    @always(write_edge(wclk))
    def write():
        if write_active(we):
            assert len(_fifo) == size
            if threshold and not full:
                _fifo.insert(0, int(data))
                size.next = size + 1

            if threshold and len(_fifo) >= depth - threshold:
                afull.next = True
            else:
                afull.next = False
            if depth and len(_fifo) >= depth:
                full.next = True
            else:
                full.next = False
            if threshold and len(_fifo) <= threshold:
                aempty.next = True
            else:
                aempty.next = False
            if len(_fifo) == 0:
                empty.next = True
            else:
                empty.next = False

            #print '++FIFO %s: size=%d, full=%d, empty=%d, afull=%d, aempty=%d' % (hex(data), size.next, full.next, empty.next, afull.next, aempty.next)


    @always(read_edge(rclk))
    def read():
        if read_active(re):
            assert len(_fifo) == size
            if empty:
                Q.next = 0
            else:
                Q.next = _fifo.pop()
                size.next = size - 1

            if threshold and len(_fifo) >= depth - threshold:
                afull.next = True
            else:
                afull.next = False
            if depth and len(_fifo) >= depth:
                full.next = True
            else:
                full.next = False
            if threshold and len(_fifo) <= threshold:
                aempty.next = True
            else:
                aempty.next = False
            if len(_fifo) == 0:
                empty.next = True
            else:
                empty.next = False

            #print '--FIFO: size=%d, full=%d, empty=%d, afull=%d, aempty=%d' % (size.next, full.next, empty.next, afull.next, aempty.next)

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
    .AFULL(${afull})
);
'''

def fifo_stimulus(reset,
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
         width=None,
         depth=None,
         write_active=active_high,
         write_edge=posedge,
         read_active=active_high,
         read_edge=posedge,
         reset_active=active_low,
         reset_edge=posedge,
         threshold=None,
         max_threshold=None):

    script = [
        ('w', 1),
        ('r', 0),
        ('r', 1),
    ]

    @instance
    def stimulus():
        for s in script:
            print s
            yield delay(10)
            if (we):
                we.next = False
                wclk.next = False
            elif (re):
                print Q, s
                assert Q == s[1]
                print 'all good in the hood'
                re.next = False
                rclk.next = False

            yield delay(10)

            if s[0] == 'w':
                we.next = True
                wclk.next = True
                data.next = s[1]
            elif s[0] == 'r':
                re.next = True
                rclk.next = True
            elif s[0] == 'x':
                pass

    return instances()

T_9600 = int(1e9 / 9600)

def fifo_write(tx_data,
         reset,
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
         width=None,
         depth=None,
         write_active=active_high,
         write_edge=posedge,
         read_active=active_high,
         read_edge=posedge,
         reset_active=active_low,
         reset_edge=posedge,
         threshold=None,
         max_threshold=None,
         duration=T_9600):
    
    print '-- Writing %s --' % hex(tx_data)
    print 'Write: start'
    we.next = True #write_active()
    wclk.next = True
    data.next = intbv(tx_data)
    yield delay(duration)

    print 'Write: stop'
    we.next = False #write_active()
    wclk.next = False
    yield delay(duration)


def fifo_read(
         reset,
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
         width=None,
         depth=None,
         write_active=active_high,
         write_edge=posedge,
         read_active=active_high,
         read_edge=posedge,
         reset_active=active_low,
         reset_edge=posedge,
         threshold=None,
         max_threshold=None,
         duration=T_9600):
    
    yield delay(duration // 2)
    print 'Read: start'
    re.next = True #write_active()
    rclk.next = True

    yield delay(duration)

    re.next = False #write_active()
    rclk.next = False
    yield delay(duration)
    print 'Read: stop'
    print '-- Received %s -- ' % hex(Q)


def tkgen(G, S, *args, **kwargs):
    s = myhdl.Simulation(G(*args, **kwargs), myhdl.traceSignals(S, *args, **kwargs))
    s.run(10*T_9600)
    myhdl.toVerilog(G, *args, **kwargs)

if __name__ == '__main__':
    width=32

    reset = Signal(bool())

    # Read
    re = Signal(bool())
    rclk = Signal(bool())
    Q = Signal(intbv(0)[width:])

    # Write
    we = Signal(bool())
    wclk = Signal(bool())
    data = Signal(intbv(0)[width:])

    # Threshold
    full = Signal(bool())
    afull = Signal(bool())
    empty = Signal(bool())
    aempty = Signal(bool())
    
    def stimulus(*args, **kwargs):
        reset, re, rclk, Q, we, wclk, data, full, afull, empty, aempty = args
        testvals = (0x01, 0x02, 0x03)

        while not full:
            yield fifo_write(random.choice(testvals), *args, **kwargs)

        while not empty:
            yield fifo_read(*args, **kwargs)
    
    args = [
         reset,
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
    ]

    kwargs = dict(
         width=width,
         depth=100
    )

    s = myhdl.Simulation(
        myhdl.traceSignals(fifo, *args, **kwargs),
        stimulus(*args, **kwargs),
    )
    s.run(1e3*T_9600)
    myhdl.toVerilog(fifo, *args, **kwargs)
