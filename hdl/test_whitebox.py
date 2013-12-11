from math import sin, pi
import os
import struct
import tempfile
import unittest

import numpy as np
from scipy import signal
import matplotlib.pyplot as plt
from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        delay, now, Simulation, StopSimulation, traceSignals, \
        Cosimulation

from apb3_utils import Apb3Bus
from fifo import fifo
from whitebox import whitebox
from rfe import WHITEBOX_STATUS_REGISTER, WHITEBOX_REGISTER_FILE
from test_dsp import figure_discrete_quadrature, figure_fft_power, figure_fft_phase
from dds import freq_to_fcw

for name, bit in WHITEBOX_STATUS_REGISTER.iteritems():
    globals()[name] = intbv(1 << bit)[32:]

for name, addr in WHITEBOX_REGISTER_FILE.iteritems():
    globals()[name] = addr

APB3_DURATION = int(1e9 / 40e6)

class WhiteboxSim(object):
    def __init__(self, bus):
        self.bus = bus
        self.clear_enable = Signal(bool(0))
        self.dac2x_clock = Signal(bool(0))
        self.dac_clock = Signal(bool(0))
        self.dac_data = Signal(intbv(0)[10:])
        self.dac_en = Signal(bool(0))
        self.adc_idata = Signal(intbv(0, min=-2**9, max=2**9))
        self.adc_qdata = Signal(intbv(0, min=-2**9, max=2**9))
        self.tx_status_led = Signal(bool(0))
        self.tx_dmaready = Signal(bool(1))
        self.rx_status_led = Signal(bool(0))
        self.rx_dmaready = Signal(bool(1))
        self.tx_q = []
        self.tx_i = []
        self.tx_n = []

    def simulate(self, stimulus, whitebox, **kwargs):
        record_tx = kwargs.get('record_tx', None)
        auto_stop = kwargs.get('auto_stop', False)
        self.sample_rate = kwargs.get('sample_rate', 10e6)
        DAC2X_DURATION = int(1e9 / (self.sample_rate * 2))
        DAC_DURATION = int(1e9 / self.sample_rate)

        @instance
        def dac2x_clock():
            while True:
                self.dac2x_clock.next = not self.dac2x_clock
                yield delay(DAC2X_DURATION // 2)

        @instance
        def dac_clock():
            yield delay(DAC2X_DURATION // 4)
            while True:
                self.dac_clock.next = not self.dac_clock
                yield delay(DAC_DURATION // 2)

        @always(self.dac_clock.posedge,
                self.dac_clock.negedge,
                self.bus.presetn.negedge)
        def tx_recorder():
            if not self.bus.presetn:
                self.tx_q = []
                self.tx_i = []
                self.tx_n = []
            elif len(self.tx_i) == len(self.tx_q) and len(self.tx_i) == record_tx:
                if auto_stop:
                    raise StopSimulation
            elif self.dac_en:
                if self.dac_clock:
                    self.tx_q.append(int(self.dac_data[:]))
                    self.tx_n.append(now())
                else:
                    self.tx_i.append(int(self.dac_data[:]))


        traced = traceSignals(whitebox)
        ss = [dac_clock, dac2x_clock, stimulus, traced]
        if 'record_tx' in kwargs:
            ss.append(tx_recorder)
        s = Simulation(ss)
        s.run()

    def fft_tx(self, decim=1):
        y = [i + 1j * q for i, q in zip (self.tx_i, self.tx_q)]
        #y = signal.decimate(y, decim)
        n = len(y)
        frq = np.fft.fftfreq(n, 1/(self.sample_rate/decim))
        Y = np.fft.fft(y)#/n
        #return np.concatenate((frq[0:1+64], frq[n-64:])), \
        #       np.concatenate((Y[0:1+64], Y[n-64:]))
        return frq, Y

    def plot_tx(self, name):
        f_parent = plt.figure(name + "_tx")
        f_parent.subplots_adjust(hspace=.5)
        #plt.title(name + "_tx")

        n = len(self.tx_i)
        k = np.arange(n)

        f1 = figure_discrete_quadrature("Signal", (3, 1, 1), f_parent,
                self.dac_data, k, self.tx_i, self.tx_q)
                
        #frq = np.fft.fftfreq(n, 1/sample_rate)
        #y = [i + 1j * q for i, q in zip (self.tx_i, self.tx_q)]
        #Y = np.fft.fft(y)/n
        frq, Y = self.fft_tx()

        f2 = figure_fft_power("Power", (3, 1, 2), f_parent,
                frq, Y)

        f3 = figure_fft_phase("Phase", (3, 1, 3), f_parent,
                frq, Y)
     
    def cosim_dut(self, cosim_name, fifo_args, whitebox_args):
        bus_pclk = self.bus.pclk
        bus_paddr = self.bus.paddr
        bus_psel = self.bus.psel
        bus_penable = self.bus.penable
        bus_pwrite = self.bus.pwrite
        bus_pwdata = self.bus.pwdata
        bus_pslverr = self.bus.pslverr
        bus_pready = self.bus.pready
        bus_prdata = self.bus.prdata

        clearn = Signal(bool(1))

        fifo_depth = fifo_args['depth']

        tx_fifo_re = Signal(bool(False))
        tx_fifo_rclk = self.dac_clock
        tx_fifo_rdata = Signal(intbv(0)[32:])
        tx_fifo_we = Signal(bool(False))
        tx_fifo_wclk = self.bus.pclk
        tx_fifo_wdata = Signal(intbv(0)[32:])
        tx_fifo_full = Signal(bool(False))
        tx_fifo_afull = Signal(bool(False))
        tx_fifo_empty = Signal(bool(False))
        tx_fifo_aempty = Signal(bool(False))
        tx_fifo_afval = Signal(intbv(fifo_depth)[12:])
        tx_fifo_aeval = Signal(intbv(0)[12:])
        tx_fifo_signals = (
            clearn,
            tx_fifo_re,
            tx_fifo_rclk,
            tx_fifo_rdata,
            tx_fifo_we,
            tx_fifo_wclk,
            tx_fifo_wdata,
            tx_fifo_full,
            tx_fifo_afull,
            tx_fifo_empty,
            tx_fifo_aempty,
            tx_fifo_afval,
            tx_fifo_aeval)
        tx_fifo = fifo(*tx_fifo_signals, **fifo_args)

        rx_fifo_depth = fifo_args['depth']
        rx_fifo_re = Signal(bool(False))
        rx_fifo_rclk = self.dac_clock
        rx_fifo_rdata = Signal(intbv(0)[32:])
        rx_fifo_we = Signal(bool(False))
        rx_fifo_wclk = self.bus.pclk
        rx_fifo_wdata = Signal(intbv(0)[32:])
        rx_fifo_full = Signal(bool(False))
        rx_fifo_afull = Signal(bool(False))
        rx_fifo_empty = Signal(bool(False))
        rx_fifo_aempty = Signal(bool(False))
        rx_fifo_afval = Signal(intbv(fifo_depth)[12:])
        rx_fifo_aeval = Signal(intbv(0)[12:])
        rx_fifo_signals = (
            clearn,
            rx_fifo_re,
            rx_fifo_rclk,
            rx_fifo_rdata,
            rx_fifo_we,
            rx_fifo_wclk,
            rx_fifo_wdata,
            rx_fifo_full,
            rx_fifo_afull,
            rx_fifo_empty,
            rx_fifo_aempty,
            rx_fifo_afval,
            rx_fifo_aeval)
        rx_fifo = fifo(*rx_fifo_signals, **fifo_args)

        config_file = tempfile.NamedTemporaryFile(delete=False)
        config_file.write('+define+COSIM_NAME="%s"' % (cosim_name,))
        config_file.close()

        cmd = 'iverilog -o %s.v -c %s whitebox.v whitebox_reset.v /home/testa/whitebox/hdl/cosim_whitebox.v' % (cosim_name, config_file.name)
        os.system(cmd)

        whitebox_test = Cosimulation(
                    'vvp -m ./myhdl.vpi %s.v' % (cosim_name, ),
                    resetn=self.bus.presetn,
                    pclk=self.bus.pclk,
                    paddr=self.bus.paddr,
                    psel=self.bus.psel,
                    penable=self.bus.penable,
                    pwrite=self.bus.pwrite,
                    pwdata=self.bus.pwdata,
                    pready=self.bus.pready,
                    prdata=self.bus.prdata,
                    #pslverr=self.bus.pslverr,
                    clearn=clearn,
                    clear_enable=self.clear_enable,
                    dac_clock=self.dac_clock,
                    dac2x_clock=self.dac2x_clock,
                    dac_en=self.dac_en,
                    dac_data=self.dac_data,
                    adc_idata=self.adc_idata,
                    adc_qdata=self.adc_qdata,
                    tx_status_led=self.tx_status_led,
                    tx_dmaready=self.tx_dmaready,
                    rx_status_led=self.rx_status_led,
                    rx_dmaready=self.rx_dmaready,
                    tx_fifo_re=tx_fifo_re,
                    tx_fifo_rclk=tx_fifo_rclk,
                    tx_fifo_rdata=tx_fifo_rdata,
                    tx_fifo_we=tx_fifo_we,
                    tx_fifo_wclk=tx_fifo_wclk,
                    tx_fifo_wdata=tx_fifo_wdata,
                    tx_fifo_full=tx_fifo_full,
                    tx_fifo_afull=tx_fifo_afull,
                    tx_fifo_empty=tx_fifo_empty,
                    tx_fifo_aempty=tx_fifo_aempty,
                    tx_fifo_afval=tx_fifo_afval,
                    tx_fifo_aeval=tx_fifo_aeval,
                    rx_fifo_re=rx_fifo_re,
                    rx_fifo_rclk=rx_fifo_rclk,
                    rx_fifo_rdata=rx_fifo_rdata,
                    rx_fifo_we=rx_fifo_we,
                    rx_fifo_wclk=rx_fifo_wclk,
                    rx_fifo_wdata=rx_fifo_wdata,
                    rx_fifo_full=rx_fifo_full,
                    rx_fifo_afull=rx_fifo_afull,
                    rx_fifo_empty=rx_fifo_empty,
                    rx_fifo_aempty=rx_fifo_aempty,
                    rx_fifo_afval=rx_fifo_afval,
                    rx_fifo_aeval=rx_fifo_aeval,
        )
        return tx_fifo, rx_fifo, whitebox_test

def whitebox_clear(bus):
    yield bus.transmit(WE_STATUS_ADDR, WS_CLEAR)
    yield bus.receive(WE_STATUS_ADDR)
    while bus.rdata & WS_CLEAR:
        yield bus.receive(WE_STATUS_ADDR)

class TestApb3Transaction(unittest.TestCase):
    def test_apb3_transaction(self):
        bus = Apb3Bus(duration=APB3_DURATION)

        s = WhiteboxSim(bus)

        fifo_args = {'width': 32, 'depth': 1024}
        whitebox_args = {'interp': 200,}
        def test_whitebox_apb3_transaction():
            return s.cosim_dut("cosim_whitebox_apb3_transaction",
                    fifo_args, whitebox_args)

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

        s.simulate(stimulus, test_whitebox_apb3_transaction)

class TestDDS(unittest.TestCase):
    def test_dds(self):
        bus = Apb3Bus(duration=APB3_DURATION)
        s = WhiteboxSim(bus)
        output_n, output_i, output_q = [], [], []

        fifo_args = {'width': 32, 'depth': 1024}
        whitebox_args = {'interp': 200,}

        def test_whitebox_dds():
            return s.cosim_dut("cosim_whitebox_dds", fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()

            # Send a clear
            yield whitebox_clear(bus)

            FCW=freq_to_fcw(100e3)
            yield bus.transmit(WE_FCW_ADDR, FCW)
            yield bus.receive(WE_FCW_ADDR)
            assert bus.rdata == FCW

            ## Now start dds
            yield bus.transmit(WE_STATUS_ADDR, WES_DDSEN)
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_DDSEN

            while True:
                # Recorder will stop the simulation
                yield bus.delay(100)

        s.simulate(stimulus, test_whitebox_dds, record_tx=2**9, auto_stop=True)

        s.plot_tx("whitebox_dds")
        plt.savefig("test_whitebox_dds.png")

        # TODO: use a figure of merit for this check
        frq, Y = s.fft_tx()
        bin_spacing = frq[1] - frq[0]
        bins = 0
        local_maximum = 0
        for f, m in zip(frq, abs(Y)**2)[1:]:
            if m > local_maximum:
                local_maximum = m
            else:
                bins = bins + 1
                assert abs(f - 100e3) < bin_spacing
                break
        assert bins == 1

class TestOverrunUnderrun(unittest.TestCase):
    def test_overrun_underrun(self):
        INTERP = 200
        FIFO_DEPTH = 4
        bus = Apb3Bus(duration=APB3_DURATION)

        s = WhiteboxSim(bus)

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        whitebox_args = { 'interp': INTERP }

        def test_whitebox_overrun_underrun():
            return s.cosim_dut("cosim_whitebox_overrun_underrun",
                    fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield whitebox_clear(bus)

            # Check the fifo flags
            yield bus.receive(WE_STATUS_ADDR)
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
            while not (bus.rdata & 0x0000ffff):
                x = intbv(int(sin(1000 * (2 * pi) * N / 50000) * 2**15), min=-2**15, max=2**15)[16:]
                yield bus.transmit(WE_SAMPLE_ADDR, concat(x, x))
                N.next = N + 1
                yield bus.receive(WE_RUNS_ADDR)

            # Check that we're full
            yield bus.receive(WE_STATUS_ADDR)
            assert not (bus.rdata & WES_SPACE)
            assert bus.rdata & WES_DATA

            ## Now start transmitting
            yield bus.transmit(WE_STATUS_ADDR, WES_TXEN)
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_TXEN

            ## Wait until underrun
            yield bus.receive(WE_RUNS_ADDR)
            while not (bus.rdata & 0xffff0000):
                yield bus.delay(1000)
                yield bus.receive(WE_RUNS_ADDR)

            ## Make sure we're both over and underrun
            assert bus.rdata & 0xffff0000 and bus.rdata & 0x0000ffff

            # Check the fifo flags
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_overrun_underrun)

class TestHalt(unittest.TestCase):
    def test_halt(self):
        INTERP = 1
        FIFO_DEPTH = 64
        BULK_SIZE = 16
        CNT = 512
        bus = Apb3Bus(duration=APB3_DURATION)
        s = WhiteboxSim(bus)

        sample_rate = 10.24e6
        freq = 100e3
        n = np.arange(0, CNT)
        x = (np.cos(freq * (2 * pi) * n / sample_rate) * (2**15-1)) + \
            (np.sin(freq * (2 * pi) * n / sample_rate) * (2**15-1)) * 1j;

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        whitebox_args = { 'interp': INTERP }

        def test_whitebox_halt():
            return s.cosim_dut("cosim_whitebox_halt",
                    fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield whitebox_clear(bus)

            # Set the threshold
            afval = intbv(FIFO_DEPTH - BULK_SIZE)[16:]
            aeval = intbv(BULK_SIZE)[16:]
            yield bus.transmit(WE_THRESHOLD_ADDR, concat(afval, aeval))

            # Check the fifo flags
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            yield bus.transmit(WE_INTERP_ADDR, INTERP)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == INTERP

            def quadrature_bit_vector(x, N):
                return \
                intbv(int(x[int(N)].real), min=-2**15, max=2**15)[16:], \
                intbv(int(x[int(N)].imag), min=-2**15, max=2**15)[16:]

            ## Insert some samples
            for j in range(BULK_SIZE):
                i, q = quadrature_bit_vector(x, N)
                yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                N.next = N + 1

            ## Now start transmitting
            yield bus.transmit(WE_STATUS_ADDR, WES_TXEN)
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_TXEN

            ## Insert some more samples
            while len(n) - N > BULK_SIZE-1:
                ## Make sure there were no overruns or underruns
                yield bus.receive(WE_RUNS_ADDR)
                assert bus.rdata == 0

                ## Wait for space
                yield bus.receive(WE_STATUS_ADDR)
                while bus.rdata & WES_AFULL:
                    yield bus.delay(2)
                    yield bus.receive(WE_STATUS_ADDR)

                for j in range(BULK_SIZE):
                    i, q = quadrature_bit_vector(x, N)
                    yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                    N.next = N + 1

            ## Insert remaining samples
            while N < len(n)-1:
                ## Wait for space
                yield bus.receive(WE_STATUS_ADDR)
                while not (bus.rdata & WES_SPACE):
                    yield bus.delay(2)
                    yield bus.receive(WE_STATUS_ADDR)

                i, q = quadrature_bit_vector(x, N)
                yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                N.next = N + 1

            ## Stop the transmission
            yield bus.transmit(WE_STATUS_ADDR, WES_TXSTOP)

            ## Wait for TXEN to go low
            yield bus.receive(WE_STATUS_ADDR)
            while bus.rdata & WES_TXEN:
                yield bus.delay(2)
                yield bus.receive(WE_STATUS_ADDR)

            ## Make sure there were no overruns or underruns
            yield bus.receive(WE_RUNS_ADDR)
            assert bus.rdata == 0

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_halt, sample_rate=sample_rate,
                record_tx=CNT)

        s.plot_tx("whitebox_halt")
        plt.savefig("test_whitebox_halt.png")

class TestCic(unittest.TestCase):
    def test_cic(self):
        interp = 20
        FIFO_DEPTH = 64
        BULK_SIZE = 16
        CNT = 512
        bus = Apb3Bus(duration=APB3_DURATION)
        s = WhiteboxSim(bus)

        input_sample_rate = 500e3
        output_sample_rate = 10e6
        interp = int(output_sample_rate / input_sample_rate)
        assert interp == 20
        freq = 100e3
        n = np.arange(0, CNT/interp)
        x = (np.cos(freq * (2 * pi) * n / input_sample_rate) * (2**15-1)) + \
            (np.sin(freq * (2 * pi) * n / input_sample_rate) * (2**15-1)) * 1j;

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        whitebox_args = { 'interp': interp }

        def test_whitebox_cic():
            return s.cosim_dut("cosim_whitebox_cic",
                    fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield whitebox_clear(bus)

            # Set the threshold
            afval = intbv(FIFO_DEPTH - BULK_SIZE)[16:]
            aeval = intbv(BULK_SIZE)[16:]
            yield bus.transmit(WE_THRESHOLD_ADDR, concat(afval, aeval))

            # Check the fifo flags
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            yield bus.transmit(WE_INTERP_ADDR, interp)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == interp

            def quadrature_bit_vector(x, N):
                return \
                intbv(int(x[int(N)].real), min=-2**15, max=2**15)[16:], \
                intbv(int(x[int(N)].imag), min=-2**15, max=2**15)[16:]

            ## Insert some samples
            for j in range(BULK_SIZE):
                i, q = quadrature_bit_vector(x, N)
                yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                N.next = N + 1

            ## Now start transmitting
            yield bus.transmit(WE_STATUS_ADDR, WES_TXEN | WES_FILTEREN)
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_TXEN

            ## Insert some more samples
            while len(n) - N > BULK_SIZE-1:
                ## Make sure there were no overruns or underruns
                yield bus.receive(WE_RUNS_ADDR)
                assert bus.rdata == 0

                ## Wait for space
                yield bus.receive(WE_STATUS_ADDR)
                while bus.rdata & WES_AFULL:
                    yield bus.delay(2)
                    yield bus.receive(WE_STATUS_ADDR)

                for j in range(BULK_SIZE):
                    i, q = quadrature_bit_vector(x, N)
                    yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                    N.next = N + 1

            ## Insert remaining samples
            while N < len(n)-1:
                ## Wait for space
                yield bus.receive(WE_STATUS_ADDR)
                while not (bus.rdata & WES_SPACE):
                    yield bus.delay(2)
                    yield bus.receive(WE_STATUS_ADDR)

                i, q = quadrature_bit_vector(x, N)
                yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                N.next = N + 1

            ## Stop the transmission
            yield bus.transmit(WE_STATUS_ADDR, WES_TXSTOP)

            ## Wait for TXEN to go low
            yield bus.receive(WE_STATUS_ADDR)
            while bus.rdata & WES_TXEN:
                yield bus.delay(2)
                yield bus.receive(WE_STATUS_ADDR)

            ## Make sure there were no overruns or underruns
            yield bus.receive(WE_RUNS_ADDR)
            assert bus.rdata == 0

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_cic, sample_rate=output_sample_rate,
                record_tx=CNT)

        s.plot_tx("whitebox_cic")
        plt.savefig("test_whitebox_cic.png")

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

        s = WhiteboxSim(bus)

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        whitebox_args = { 'fifo_depth': FIFO_DEPTH, 'interp': INTERP }

        def test_whitebox_pipe_samples():
            return s.cosim_dut("cosim_whitebox_pipe_samples",
                    fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

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
            yield whitebox_clear(bus)

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
                #yield bus.transmit(WE_STATUS_ADDR, WS_CLEAR_TXIRQ)

                ######## TXSTART HELPER ###########
                # If almost full, start the transmit
                yield bus.receive(WE_STATUS_ADDR)
                if bus.rdata & WES_AFULL:
                    yield bus.transmit(WE_STATUS_ADDR, WES_TXEN | WES_FILTEREN)

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
                yield bus.receive(WE_STATUS_ADDR)
                if bus.rdata & WES_TXEN:
                    pass
                    # TODO!!
                    #while not txirq:
                    #    yield bus.delay(10*1024)

            # And turn off the transmitter
            yield bus.transmit(WE_STATUS_ADDR, WES_TXSTOP)
            yield bus.receive(WE_STATUS_ADDR)
            while bus.rdata & WES_TXEN:
                yield bus.receive(WE_STATUS_ADDR)

            yield bus.delay(40)
            assert not s.dac_en

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_pipe_samples)

if __name__ == '__main__':
    unittest.main()
