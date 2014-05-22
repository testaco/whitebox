import unittest

from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        traceSignals, Simulation, delay, StopSimulation
import numpy as np

def ram4k9(reset,
           addra,
           dina,
           widtha0,
           widtha1,
           pipea,
           wmodea,
           blka,
           wena,
           clka,
           douta,
           addrb,
           dinb,
           widthb0,
           widthb1,
           pipeb,
           wmodeb,
           blkb,
           wenb,
           clkb,
           doutb,
           **kwargs):
    
    """An Actel RAM4K9 block."""
    if not widtha1 and not widtha0:
        widtha = 1
        deptha = 4096
    elif not widtha1 and widtha0:
        widtha = 2
        deptha = 2048
    elif widtha1 and not widtha0:
        widtha = 4
        deptha = 1024
    elif widtha1 and widtha0:
        widtha = 9
        deptha = 512

    if not widthb1 and not widthb0:
        widthb = 1
        depthb = 4096
    elif not widthb1 and widthb0:
        widthb = 2
        depthb = 2048
    elif widthb1 and not widthb0:
        widthb = 4
        depthb = 1024
    elif widthb1 and widthb0:
        widthb = 9
        depthb = 512
    
    if widtha != widthb or deptha != depthb:
        raise Exception, "Doesn't support different widths/depths on two ports"

    #_ram = [Signal(intbv(0, min=-2**(widtha-1), max=2**(widtha-1))) for i in xrange(deptha)]
    _ram = []
    for i in xrange(deptha):
        d = 0
        if 'data' in kwargs and len(kwargs['data']) > i:
            d = kwargs['data'][i]
        _ram.append(Signal(intbv(d, min=0, max=2**widtha)))

    ba = len(douta)
    bb = len(doutb)
    da = Signal(intbv(0, min=0, max=2**ba))
    db = Signal(intbv(0, min=0, max=2**bb))

    @always_seq(clka.posedge, reset=reset)
    def porta():
        if not blka:
            if wena: # Read
                if pipea:
                    da.next = _ram[int(addra)]
                    douta.next = da
                else:
                    douta.next = _ram[int(addra)]
            else: # Write
                _ram[int(addra)].next = intbv(int(dina))
                if wmodea:
                    douta.next = intbv(int(dina))
                

    @always_seq(clkb.posedge, reset=reset)
    def portb():
        if not blkb:
            if wenb: # Read
                if pipeb:
                    db.next = _ram[int(addrb)]
                    doutb.next = db
                else:
                    doutb.next = _ram[int(addrb)]
            else: # Write
                _ram[int(addrb)].next = intbv(int(dinb))
                if wmodeb:
                    doutb.next = intbv(int(dinb))

    return porta, portb

ram4k9.verilog_code = '''
RAM4K9 RAM4K9_0(
    .RESET(${reset}),
    .ADDRA(${addra}),
    .DINA(${dina}),
    .WIDTHA0(${widtha0}),
    .WIDTHA1(${widtha1}),
    .PIPEA(${pipea}),
    .WMODEA(${wmodea}),
    .BLKA(${blka}),
    .WENA(${wena}),
    .CLKA(${clka}),
    .DOUTA(${douta}),
    .ADDRB(${addrb}),
    .DINB(${dinb}),
    .WIDTHB0(${widthb0}),
    .WIDTHB1(${widthb1}),
    .PIPEB(${pipeb}),
    .WMODEB(${wmodeb}),
    .BLKB(${blkb}),
    .WENB(${wenb}),
    .CLKB(${clkb}),
    .DOUTB(${doutb})
);
'''

class RamPort(object):
    def __init__(self, addr, din, width0, width1, pipe, wmode, blk, wen, clk, dout):
        self.addr = addr
        self.din = din
        self.width0 = width0
        self.width1 = width1
        self.pipe = pipe
        self.wmode = wmode
        self.blk = blk
        self.wen = wen
        self.clk = clk
        self.dout = dout

    def signals(self):
        return self.addr, self.din, self.width0, self.width1, self.pipe, self.wmode, self.blk, self.wen, self.clk, self.dout

class Ram(object):
    def __init__(self, resetn, clka, clkb, **kwargs):
        self.resetn = resetn
        self.addra = Signal(intbv(0, min=0, max=2**9))
        self.dina = Signal(intbv(0, min=0, max=2**9))
        self.widtha1 = Signal(bool(1))
        self.widtha0 = Signal(bool(1))
        self.pipea = Signal(bool(1))
        self.wmodea = Signal(bool(0))
        self.blka = Signal(bool(1))
        self.wena = Signal(bool(1))
        self.clka = clka
        self.douta = Signal(intbv(0, min=0, max=2**9))
        self.addrb = Signal(intbv(0, min=0, max=2**9))
        self.dinb = Signal(intbv(0, min=0, max=2**9))
        self.widthb1 = Signal(bool(1))
        self.widthb0 = Signal(bool(1))
        self.pipeb = Signal(bool(1))
        self.wmodeb = Signal(bool(0))
        self.blkb = Signal(bool(1))
        self.wenb = Signal(bool(1))
        self.clkb = clkb
        self.doutb = Signal(intbv(0, min=0, max=2**9))
        self.ram = ram4k9(reset=self.resetn,
                addra=self.addra,
                dina=self.dina,
                widtha0=self.widtha0,
                widtha1=self.widtha1,
                pipea=self.pipea,
                wmodea=self.wmodea,
                blka=self.blka,
                wena=self.wena,
                clka=self.clka,
                douta=self.douta,
                addrb=self.addrb,
                dinb=self.dinb,
                widthb0=self.widthb0,
                widthb1=self.widthb1,
                pipeb=self.pipeb,
                wmodeb=self.wmodeb,
                blkb=self.blkb,
                wenb=self.wenb,
                clkb=self.clkb,
                doutb=self.doutb,
                **kwargs)
        self.port = { 'a': RamPort(
                self.addra,
                self.dina,
                self.widtha0,
                self.widtha1,
                self.pipea,
                self.wmodea,
                self.blka,
                self.wena,
                self.clka,
                self.douta),
                     'b': RamPort(
                self.addrb,
                self.dinb,
                self.widthb0,
                self.widthb1,
                self.pipeb,
                self.wmodeb,
                self.blkb,
                self.wenb,
                self.clkb,
                self.doutb) }

class Ram2(object):
    def __init__(self, resetn, clka, clkb, **kwargs):
        self.resetn = resetn
        self.width1 = Signal(bool(1))
        self.width0 = Signal(bool(1))
        self.pipe = Signal(bool(1))
        self.wmode = Signal(bool(0))

        self.addra = Signal(intbv(0, min=0, max=2**9))
        self.blka = Signal(bool(1))
        self.wena = Signal(bool(1))
        self.clka = clka

        self.a_dina = Signal(intbv(0, min=0, max=2**9))
        self.a_douta = Signal(intbv(0, min=0, max=2**9))
        self.b_dina = Signal(intbv(0, min=0, max=2**9))
        self.b_douta = Signal(intbv(0, min=0, max=2**9))
        
        self.addrb = Signal(intbv(0, min=0, max=2**9))
        self.blkb = Signal(bool(1))
        self.wenb = Signal(bool(1))
        self.clkb = clkb

        self.a_dinb = Signal(intbv(0, min=0, max=2**9))
        self.a_doutb = Signal(intbv(0, min=0, max=2**9))
        self.b_dinb = Signal(intbv(0, min=0, max=2**9))
        self.b_doutb = Signal(intbv(0, min=0, max=2**9))

        a_kwargs = kwargs.copy()
        b_kwargs = kwargs.copy()
        if 'data' in kwargs:
            a_kwargs['data'] = [d & 0x1ff for d in kwargs['data']]
            b_kwargs['data'] = [(d >> 9) & 0x1ff for d in kwargs['data']]

        self.rama = ram4k9(reset=self.resetn,
                addra=self.addra,
                dina=self.a_dina,
                widtha0=self.width0,
                widtha1=self.width1,
                pipea=self.pipe,
                wmodea=self.wmode,
                blka=self.blka,
                wena=self.wena,
                clka=self.clka,
                douta=self.a_douta,
                addrb=self.addrb,
                dinb=self.a_dinb,
                widthb0=self.width0,
                widthb1=self.width1,
                pipeb=self.pipe,
                wmodeb=self.wmode,
                blkb=self.blkb,
                wenb=self.wenb,
                clkb=self.clkb,
                doutb=self.a_doutb,
                **a_kwargs)
        self.ramb = ram4k9(reset=self.resetn,
                addra=self.addra,
                dina=self.b_dina,
                widtha0=self.width0,
                widtha1=self.width1,
                pipea=self.pipe,
                wmodea=self.wmode,
                blka=self.blka,
                wena=self.wena,
                clka=self.clka,
                douta=self.b_douta,
                addrb=self.addrb,
                dinb=self.b_dinb,
                widthb0=self.width0,
                widthb1=self.width1,
                pipeb=self.pipe,
                wmodeb=self.wmode,
                blkb=self.blkb,
                wenb=self.wenb,
                clkb=self.clkb,
                doutb=self.b_doutb,
                **b_kwargs)
        self.port = { 'a': RamPort(
                self.addra,
                [self.a_dina, self.b_dina],
                self.width0,
                self.width1,
                self.pipe,
                self.wmode,
                self.blka,
                self.wena,
                self.clka,
                [self.a_douta, self.b_douta]),
                     'b': RamPort(
                self.addrb,
                [self.a_dinb, self.b_dinb],
                self.width0,
                self.width1,
                self.pipe,
                self.wmode,
                self.blkb,
                self.wenb,
                self.clkb,
                [self.a_doutb, self.b_doutb]) }

class RamSimulation(object):
    def __init__(self, ram=None, duration=None):
        if ram:
            self.ram = ram
            self.resetn = self.ram.resetn
            self.clka = self.ram.port['a'].clk
            self.clkb = self.ram.port['b'].clk
        else:
            self.resetn = ResetSignal(0, 0, async=True)
            self.clka = Signal(bool(0))
            self.clkb = Signal(bool(0))
            self.ram = Ram(self.resetn, self.clka, self.clkb)

        self.duration = duration or int(1e9 / 10e6 / 2)
        self.port = self.ram.port

    def delay(self):
        port_a_clk = self.ram.port['a'].clk
        port_b_clk = self.ram.port['b'].clk
        port_a_clk.next = True
        port_b_clk.next = True
        yield delay(self.duration // 2)
        port_a_clk.next = False
        port_b_clk.next = False
        yield delay(self.duration // 2)

    def reset(self):
        print '-- Reset --'
        self.resetn.next = False
        yield self.delay()
        yield self.delay()
        self.resetn.next = True

    def write(self, port, *args):
        p = self.ram.port[port]
        p.blk.next = False

        for (addr, data) in args:
            print '-- Writing %s %s=%s --' % (port.upper(), hex(addr), hex(data))
            p.addr.next = intbv(addr)
            p.din.next = intbv(data)
            p.wen.next = False
            p.clk.next = True
            yield delay(self.duration // 2)
            p.clk.next = False
            yield delay(self.duration // 2)

        print 'Write: stop'
        p.blk.next = True

    def read(self, port, *args):
        p = self.ram.port[port]

        p.blk.next = False
        first = True
        self.result = []

        for addr in args:
            print 'Read: start %s' % hex(addr)
            p.addr.next = intbv(addr)
            p.wen.next = True
            p.clk.next = True
            yield delay(self.duration // 2)
            p.clk.next = False
            yield delay(self.duration // 2)

            if not first:
                self.result.append(intbv(int(p.dout)))
            else:
                first = False

        p.clk.next = True
        yield delay(self.duration // 2)
        p.clk.next = False
        yield delay(self.duration // 2)
        self.result.append(intbv(int(p.dout)))

        p.blk.next = True

    def simulate(self, test):
        traced = traceSignals(test)
        s = Simulation(traced)
        s.run()

class TestRam(unittest.TestCase):
    def test_ram(self):
        s = RamSimulation()

        def test_ram_interface():
            resetn = s.resetn
            port_a_addr = s.port['a'].addr
            port_a_din = s.port['a'].din
            port_a_width1 = s.port['a'].width1
            port_a_width0 = s.port['a'].width0
            port_a_pipe = s.port['a'].pipe
            port_a_wmode = s.port['a'].wmode
            port_a_blk = s.port['a'].blk
            port_a_wen = s.port['a'].wen
            port_a_clk = s.port['a'].clk
            port_a_dout = s.port['a'].dout
            port_b_addr = s.port['b'].addr
            port_b_din = s.port['b'].din
            port_b_width1 = s.port['b'].width1
            port_b_width0 = s.port['b'].width0
            port_b_pipe = s.port['b'].pipe
            port_b_wmode = s.port['b'].wmode
            port_b_blk = s.port['b'].blk
            port_b_wen = s.port['b'].wen
            port_b_clk = s.port['b'].clk
            port_b_dout = s.port['b'].dout
            @instance
            def stimulus():
                yield s.reset()
                yield s.write('a', (0, 0), (1, 1), (2, 2), (3, 3), (0, 4))
                yield s.read('b', 0, 1, 2, 3)
                assert s.result == [4, 1, 2, 3]

                yield s.write('b', (4, 5), (5, 6), (6, 7), (7, 8), (8, 9))
                yield s.read('a', 4, 5, 6, 7, 8)
                assert s.result == [5, 6, 7, 8, 9]

                raise StopSimulation

            return stimulus, s.ram.ram

        s.simulate(test_ram_interface)

if __name__ == '__main__':
    unittest.main()
