from math import sin, pi
import os
import struct
import tempfile
import unittest

from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        delay, now, Simulation, StopSimulation, traceSignals, \
        Cosimulation

from apb3_utils import Apb3Bus
from fifo import fifo
from exciter import exciter
from rfe import \
    WE_SAMPLE_ADDR, WE_STATE_ADDR, WE_INTERP_ADDR, WE_FCW_ADDR, \
    WE_RUNS_ADDR, WE_THRESHOLD_ADDR, WE_CORRECTION_ADDR, \
    WES_CLEAR, WES_TXSTOP, WES_TXEN, WES_DDSEN, WES_FILTEREN, \
    WES_AEMPTY, WES_AFULL, WES_DATA, WES_SPACE

FCW=int(200e6)

APB3_DURATION = int(1e9 / 20e6)
DAC_DURATION = int(1e9 / 10e6)
DAC2X_DURATION = int(1e9 / 20e6)

class ExciterSim(object):
    def __init__(self, bus):
        self.bus = bus
        self.dac2x_clock = Signal(bool(0))
        self.dac_clock = Signal(bool(0))
        self.dac_data = Signal(intbv(0)[10:])
        self.dac_en = Signal(bool(0))
        self.status_led = Signal(bool(0))
        self.dmaready = Signal(bool(1))
        self.txirq = Signal(bool(0))
        self.clear_enable = Signal(bool(0))

    def simulate(self, stimulus, exciter, **kwargs):
        @always(delay(DAC2X_DURATION // 2))
        def dac2x_clock():
            self.dac2x_clock.next = not self.dac2x_clock

        @always(delay(DAC_DURATION // 2))
        def dac_clock():
            self.dac_clock.next = not self.dac_clock

        traced = traceSignals(exciter)
        s = Simulation(dac_clock, dac2x_clock, stimulus, traced)
        s.run()
     
    def cosim_dut(self, cosim_name, fifo_args, exciter_args):
        bus_pclk = self.bus.pclk
        bus_paddr = self.bus.paddr
        bus_psel = self.bus.psel
        bus_penable = self.bus.penable
        bus_pwrite = self.bus.pwrite
        bus_pwdata = self.bus.pwdata
        bus_pslverr = self.bus.pslverr
        bus_pready = self.bus.pready
        bus_prdata = self.bus.prdata

        fifo_depth = fifo_args['depth']
        fifo_re = Signal(bool(False))
        fifo_rclk = self.dac_clock
        fifo_rdata = Signal(intbv(0)[32:])
        fifo_we = Signal(bool(False))
        fifo_wclk = self.bus.pclk
        fifo_wdata = Signal(intbv(0)[32:])
        fifo_full = Signal(bool(False))
        fifo_afull = Signal(bool(False))
        fifo_empty = Signal(bool(False))
        fifo_aempty = Signal(bool(False))
        fifo_afval = Signal(intbv(fifo_depth)[12:])
        fifo_aeval = Signal(intbv(0)[12:])
        clearn = Signal(bool(1))

        fifo_signals = (
            clearn,
            fifo_re,
            fifo_rclk,
            fifo_rdata,
            fifo_we,
            fifo_wclk,
            fifo_wdata,
            fifo_full,
            fifo_afull,
            fifo_empty,
            fifo_aempty,
            fifo_afval,
            fifo_aeval)
        fifo_0 = fifo(*fifo_signals, **fifo_args)

        config_file = tempfile.NamedTemporaryFile(delete=False)
        config_file.write('+define+COSIM_NAME="%s"' % (cosim_name,))
        config_file.close()

        cmd = 'iverilog -o %s.v -c %s exciter.v exciter_reset.v /home/testa/whitebox/hdl/cosim_exciter.v' % (cosim_name, config_file.name)
        os.system(cmd)

        exciter_0 = Cosimulation('vvp -m ./myhdl.vpi %s.v' % (cosim_name, ),
                    resetn=self.bus.presetn,
                    clearn=clearn,
                    dac2x_clock=self.dac2x_clock,
                    pclk=self.bus.pclk,
                    paddr=self.bus.paddr,
                    psel=self.bus.psel,
                    penable=self.bus.penable,
                    pwrite=self.bus.pwrite,
                    pwdata=self.bus.pwdata,
                    pready=self.bus.pready,
                    prdata=self.bus.prdata,
                    pslverr=self.bus.pslverr,
                    dac_clock=self.dac_clock,
                    dac_data=self.dac_data,
                    dac_en=self.dac_en,
                    status_led=self.status_led,
                    dmaready=self.dmaready,
                    txirq=self.txirq,
                    clear_enable=self.clear_enable,
                    fifo_re=fifo_re,
                    fifo_rclk=fifo_rclk,
                    fifo_rdata=fifo_rdata,
                    fifo_we=fifo_we,
                    fifo_wclk=fifo_wclk,
                    fifo_wdata=fifo_wdata,
                    fifo_full=fifo_full,
                    fifo_afull=fifo_afull,
                    fifo_empty=fifo_empty,
                    fifo_aempty=fifo_aempty,
                    fifo_afval=fifo_afval,
                    fifo_aeval=fifo_aeval
        )
        return fifo_0, exciter_0

class TestApb3Transaction(unittest.TestCase):
    def test_apb3_transaction(self):
        bus = Apb3Bus(duration=APB3_DURATION)

        s = ExciterSim(bus)

        fifo_args = {'width': 32, 'depth': 1024}
        exciter_args = {'interp': 200,}
        def test_exciter_apb3_transaction():
            return s.cosim_dut("cosim_exciter_apb3_transaction",
                    fifo_args, exciter_args)

        @instance
        def stimulus():
            yield bus.reset()
            yield bus.transmit(WE_INTERP_ADDR, 20)
            yield bus.transmit(WE_FCW_ADDR, 100)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == 20
            yield bus.receive(WE_FCW_ADDR)
            assert bus.rdata == 100
            raise StopSimulation

        s.simulate(stimulus, test_exciter_apb3_transaction)

class TestDDS(unittest.TestCase):
    def test_dds(self):
        bus = Apb3Bus(duration=APB3_DURATION)

        s = ExciterSim(bus)

        fifo_args = {'width': 32, 'depth': 1024}
        exciter_args = {'interp': 200,}

        def test_exciter_dds():
            return s.cosim_dut("cosim_exciter_dds", fifo_args, exciter_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield bus.transmit(WE_STATE_ADDR, WES_CLEAR)

            yield bus.transmit(WE_FCW_ADDR, FCW)
            yield bus.receive(WE_FCW_ADDR)
            assert bus.rdata == FCW

            ## Now start dds
            yield bus.transmit(WE_STATE_ADDR, WES_DDSEN)
            yield bus.receive(WE_STATE_ADDR)
            assert bus.rdata & WES_DDSEN

            yield bus.delay(10000)

            raise StopSimulation

        s.simulate(stimulus, test_exciter_dds)

class TestOverrunUnderrun(unittest.TestCase):
    def test_overrun_underrun(self):
        INTERP = 200
        FIFO_DEPTH = 4
        bus = Apb3Bus(duration=APB3_DURATION)

        s = ExciterSim(bus)

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        exciter_args = { 'interp': INTERP }

        def test_exciter_overrun_underrun():
            return s.cosim_dut("cosim_exciter_overrun_underrun",
                    fifo_args, exciter_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield bus.transmit(WE_STATE_ADDR, WES_CLEAR)

            # Check the fifo flags
            yield bus.receive(WE_STATE_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            yield bus.transmit(WE_THRESHOLD_ADDR,
                concat(intbv(1)[16:], intbv(3)[16:]))
            yield bus.receive(WE_THRESHOLD_ADDR)
            assert bus.rdata == concat(intbv(1)[16:], intbv(3)[16:])

            yield bus.transmit(WE_INTERP_ADDR, INTERP)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == INTERP

            ## Insert samples until overrun
            yield bus.receive(WE_RUNS_ADDR)
            while not (bus.rdata & 0xffff0000):
                x = intbv(int(sin(1000 * (2 * pi) * N / 50000) * 2**15), min=-2**15, max=2**15)[16:]
                yield bus.transmit(WE_SAMPLE_ADDR, concat(x, x))
                N.next = N + 1
                yield bus.receive(WE_RUNS_ADDR)

            # Check that we're full
            yield bus.receive(WE_STATE_ADDR)
            assert not (bus.rdata & WES_SPACE)
            assert bus.rdata & WES_DATA

            ## Now start transmitting
            yield bus.transmit(WE_STATE_ADDR, WES_TXEN)
            yield bus.receive(WE_STATE_ADDR)
            assert bus.rdata & WES_TXEN

            ## Wait until underrun
            yield bus.receive(WE_RUNS_ADDR)
            while not (bus.rdata & 0x0000ffff):
                yield bus.delay(1000)
                yield bus.receive(WE_RUNS_ADDR)

            ## Make sure we're both over and underrun
            assert bus.rdata & 0xffff0000 and bus.rdata & 0x0000ffff

            # Check the fifo flags
            yield bus.receive(WE_STATE_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            raise StopSimulation

        s.simulate(stimulus, test_exciter_overrun_underrun)

class TestHalt(unittest.TestCase):
    def test_halt(self):
        INTERP = 200
        FIFO_DEPTH = 4
        BULK_SIZE = 2
        bus = Apb3Bus(duration=APB3_DURATION)

        s = ExciterSim(bus)

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        exciter_args = { 'interp': INTERP }

        def test_exciter_halt():
            return s.cosim_dut("cosim_exciter_halt",
                    fifo_args, exciter_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield bus.transmit(WE_STATE_ADDR, WES_CLEAR)

            # Check the fifo flags
            yield bus.receive(WE_STATE_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            yield bus.transmit(WE_INTERP_ADDR, INTERP)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == INTERP

            ## Insert some samples
            for i in range(BULK_SIZE):
                x = intbv(int(sin(1000 * (2 * pi) * N / 50000) * 2**15), min=-2**15, max=2**15)[16:]
                yield bus.transmit(WE_SAMPLE_ADDR, concat(x, x))
                N.next = N + 1

            ## Now start transmitting
            yield bus.transmit(WE_STATE_ADDR, WES_TXEN)
            yield bus.receive(WE_STATE_ADDR)
            assert bus.rdata & WES_TXEN

            ## Insert some more samples
            for i in range(BULK_SIZE):
                x = intbv(int(sin(1000 * (2 * pi) * N / 50000) * 2**15), min=-2**15, max=2**15)[16:]
                yield bus.transmit(WE_SAMPLE_ADDR, concat(x, x))
                N.next = N + 1

            ## Stop the transmission
            yield bus.transmit(WE_STATE_ADDR, WES_TXSTOP)

            ## Wait for TXEN to go low
            yield bus.receive(WE_STATE_ADDR)
            while bus.rdata & WES_TXEN:
                yield bus.delay(2)
                yield bus.receive(WE_STATE_ADDR)

            ## Make sure there were no overruns or underruns
            yield bus.receive(WE_RUNS_ADDR)
            assert bus.rdata == 0

            raise StopSimulation

        s.simulate(stimulus, test_exciter_halt)

class TestPipeSamples(unittest.TestCase):
    def test_pipe_samples(self):
        bus = Apb3Bus(duration=APB3_DURATION)
        BULK_SIZE=1 #
        FIFO_DEPTH=BULK_SIZE*4
        AFVAL = BULK_SIZE*3 #
        AEVAL = BULK_SIZE*2 #
        SAMPLES_TO_SIMULATE=FIFO_DEPTH*2
        INTERP=20
        CORRECT_I = intbv(0, min=-2**9, max=2**9)
        CORRECT_Q = intbv(0, min=-2**9, max=2**9)

        s = ExciterSim(bus)

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        exciter_args = { 'fifo_depth': FIFO_DEPTH, 'interp': INTERP }

        def test_exciter_pipe_samples():
            return s.cosim_dut("cosim_exciter_pipe_samples",
                    fifo_args, exciter_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Load the samples file
            f = open('/home/testa/whitebox/hdl/sin.samples', 'r')
            struct_fmt = 'I'
            struct_len = struct.calcsize(struct_fmt)
            assert struct_len == 4
            struct_unpack = struct.Struct(struct_fmt).unpack_from
            def read_sample():
                data = f.read(struct_len)
                if not data: raise StopSimulation
                return intbv(struct_unpack(data)[0])[32:]

            # Send a clear
            yield bus.reset()
            yield bus.transmit(WE_STATE_ADDR, WES_CLEAR)

            yield bus.transmit(WE_INTERP_ADDR, INTERP)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == INTERP

            yield bus.transmit(WE_THRESHOLD_ADDR,
                concat(intbv(AFVAL)[16:], intbv(AEVAL)[16:]))
            yield bus.receive(WE_THRESHOLD_ADDR)
            assert bus.rdata == concat(intbv(AFVAL)[16:], intbv(AEVAL)[16:])

            correct_word = concat(intbv(0)[6:], CORRECT_Q[10:],
                            intbv(0)[6:], CORRECT_I[10:])
            yield bus.transmit(WE_CORRECTION_ADDR, correct_word)
            yield bus.receive(WE_CORRECTION_ADDR)
            assert bus.rdata == correct_word

            N.next = 0

            cur_overruns = 0
            cur_underruns = 0
            while N < SAMPLES_TO_SIMULATE:
                ######### TXIRQ_HANDLER ############
                ## Acknowldege pending interrupt
                #yield bus.transmit(WE_STATE_ADDR, WES_CLEAR_TXIRQ)

                ######## TXSTART HELPER ###########
                # If almost full, start the transmit
                yield bus.receive(WE_STATE_ADDR)
                if bus.rdata & WES_AFULL:
                    yield bus.transmit(WE_STATE_ADDR, WES_TXEN | WES_FILTEREN)

                # Otherwise, insert BULK_SIZE samples
                else:
                    # Simulates the DMA controller
                    for j in range(BULK_SIZE):
                        yield bus.transmit(WE_SAMPLE_ADDR, read_sample())

                    N.next = N + BULK_SIZE
                
                ######### PROCESS ############
                # Check for overruns / underruns
                yield bus.receive(WE_RUNS_ADDR)
                overruns =  (bus.rdata & 0xffff0000) >> 16
                underruns = (bus.rdata & 0x0000ffff)

                if overruns != cur_overruns:
                    raise OverrunError
                if underruns != cur_underruns:
                    raise UnderrunError

                # Do other processing...

                ## Wait for TXIRQ to assert
                yield bus.receive(WE_STATE_ADDR)
                if bus.rdata & WES_TXEN:
                    pass
                    # TODO!!
                    #while not txirq:
                    #    yield bus.delay(10*1024)

            # And turn off the transmitter
            yield bus.transmit(WE_STATE_ADDR, WES_TXSTOP)
            yield bus.receive(WE_STATE_ADDR)
            while bus.rdata & WES_TXEN:
                yield bus.receive(WE_STATE_ADDR)

            yield bus.delay(40)
            assert not s.dac_en

            raise StopSimulation

        s.simulate(stimulus, test_exciter_pipe_samples)

if __name__ == '__main__':
    unittest.main()
