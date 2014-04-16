"""
Simulating the Whitebox SoC Peripheral
--------------------------------------
"""
from math import sin, pi, ceil, log
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
from ram import Ram, RamSimulation

for name, bit in WHITEBOX_STATUS_REGISTER.iteritems():
    globals()[name] = intbv(1 << bit)[32:]

for name, addr in WHITEBOX_REGISTER_FILE.iteritems():
    globals()[name] = addr

from rfe import WF_ACCESS_COEFFS
WF_ACCESS_COEFFS = intbv(1 << WF_ACCESS_COEFFS)[32:]

APB3_DURATION = int(1e9 / 40e6)

class WhiteboxSim(object):
    """Simulate Whitebox Peripheral and control it with a ``bus``.
    
    :param bus: The APB3Bus to connect the peripheral to.
    """
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
        pclk = self.bus.pclk
        self.fir_coeff_ram = Ram(self.bus.presetn, self.dac_clock, pclk)
        self.fir_delay_line_i_ram = Ram(self.bus.presetn, self.dac_clock, self.dac_clock)
        self.fir_delay_line_q_ram = Ram(self.bus.presetn, self.dac_clock, self.dac_clock)

    def simulate(self, stimulus, whitebox, **kwargs):
        """Acturally run the cosimulation with iverilog.
        
        :param stimulus: A callable that returns the cosim object.
        :param whitebox: A whitebox peripheral object.
        :param record_tx: Record the passed in number of valid samples.
        :param auto_stop: Raise ``StopSimulation`` when the correct number of samples have been recorded.
        :param sample_rate: Samples per second.
        """
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
        """Compute the FFT of the transmitted signal."""
        #y = [i + 1j * q for i, q in zip (self.tx_i, self.tx_q)]
        y = self.tx(decim=decim)
        #y = signal.decimate(y, decim)
        n = len(y)
        frq = np.fft.fftfreq(n, 1/(self.sample_rate/decim))
        Y = np.fft.fft(y)#/n
        #return np.concatenate((frq[0:1+64], frq[n-64:])), \
        #       np.concatenate((Y[0:1+64], Y[n-64:]))
        return frq, Y

    def tx(self, decim=1):
        re = [intbv(i)[10:] for i in self.tx_i][::decim]
        re = [intbv(concat(not i[len(i)-1], i[len(i)-1:])).signed() for i in re]
        im = [intbv(q)[10:] for q in self.tx_q][::decim]
        im = [intbv(concat(not q[len(q)-1], q[len(q)-1:])).signed() for q in im]
        y = np.array([i + 1j * q for i, q in zip(re, im)])
        return y

    def plot_tx(self, name, decim=1):
        """Plot the transmitter's output."""
        f_parent = plt.figure(name + "_tx")
        f_parent.subplots_adjust(hspace=.5)
        #plt.title(name + "_tx")

        y = self.tx(decim=decim)
        n = len(y)
        k = np.arange(n)

        f1 = figure_discrete_quadrature("Signal", (3, 1, 1), f_parent,
                self.dac_data, k, [i.real for i in y], [i.imag for i in y])
                
        #frq = np.fft.fftfreq(n, 1/sample_rate)
        #y = [i + 1j * q for i, q in zip (self.tx_i, self.tx_q)]
        #Y = np.fft.fft(y)/n
        frq, Y = self.fft_tx()

        f2 = figure_fft_power("Power", (3, 1, 2), f_parent,
                frq, Y)

        f3 = figure_fft_phase("Phase", (3, 1, 3), f_parent,
                frq, Y)
     
    def cosim_dut(self, cosim_name, fifo_args, whitebox_args):
        """Get the ``Cosimulation`` object.

        :param cosim_name: The name of the cosimulation.
        :param fifo_args: A dictionary of args to pass to the FIFOs.
        :param whitebox_args: A dictionary of args to pass to the Whitebox Peripheral.
        :returns: A `myhdl.Cosimulation` object.
        """
        bus_pclk = self.bus.pclk
        bus_paddr = self.bus.paddr
        bus_psel = self.bus.psel
        bus_penable = self.bus.penable
        bus_pwrite = self.bus.pwrite
        bus_pwdata = self.bus.pwdata
        bus_pslverr = self.bus.pslverr
        bus_pready = self.bus.pready
        bus_prdata = self.bus.prdata

        clearn = ResetSignal(0, 0, async=True)

        fifo_depth = fifo_args['depth']

        tx_fifo_re = Signal(bool(False))
        tx_fifo_rclk = self.dac_clock
        tx_fifo_rdata = Signal(intbv(0)[32:])
        tx_fifo_we = Signal(bool(False))
        tx_fifo_wclk = self.bus.pclk
        tx_fifo_wdata = Signal(intbv(0)[32:])
        tx_fifo_full = Signal(bool(False))
        tx_fifo_afull = Signal(bool(False))
        tx_fifo_empty = Signal(bool(True))
        tx_fifo_aempty = Signal(bool(True))
        tx_fifo_afval = Signal(intbv(fifo_depth - 1)[10:])
        tx_fifo_aeval = Signal(intbv(0)[10:])
        tx_fifo_wack = Signal(bool(False))
        tx_fifo_dvld = Signal(bool(False))
        tx_fifo_overflow = Signal(bool(False))
        tx_fifo_underflow = Signal(bool(False))
        tx_fifo_rdcnt = Signal(intbv(0, min=0, max=fifo_depth + 1))
        tx_fifo_wrcnt = Signal(intbv(fifo_depth, min=0, max=fifo_depth + 1))


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
            tx_fifo_aeval,
            tx_fifo_wack,
            tx_fifo_dvld,
            tx_fifo_overflow,
            tx_fifo_underflow,
            tx_fifo_rdcnt,
            tx_fifo_wrcnt)
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
        rx_fifo_empty = Signal(bool(True))
        rx_fifo_aempty = Signal(bool(True))
        rx_fifo_afval = Signal(intbv(fifo_depth - 1)[10:])
        rx_fifo_aeval = Signal(intbv(0)[10:])
        rx_fifo_wack = Signal(bool(False))
        rx_fifo_dvld = Signal(bool(False))
        rx_fifo_overflow = Signal(bool(False))
        rx_fifo_underflow = Signal(bool(False))
        rx_fifo_rdcnt = Signal(intbv(0, min=0, max=fifo_depth + 1))
        rx_fifo_wrcnt = Signal(intbv(fifo_depth, min=0, max=fifo_depth + 1))
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
            rx_fifo_aeval,
            rx_fifo_wack,
            rx_fifo_dvld,
            rx_fifo_overflow,
            rx_fifo_underflow,
            rx_fifo_rdcnt,
            rx_fifo_wrcnt)
        rx_fifo = fifo(*rx_fifo_signals, **fifo_args)

        config_file = tempfile.NamedTemporaryFile(delete=False)
        config_file.write('+define+COSIM_NAME="%s"' % (cosim_name,))
        config_file.close()

        cmd = 'iverilog -o %s.v -c %s whitebox.v whitebox_reset.v /home/testa/whitebox/hdl/cosim_whitebox.v' % (cosim_name, config_file.name)
        os.system(cmd)

        fir_coeff_ram_addr = self.fir_coeff_ram.port['a'].addr
        fir_coeff_ram_din = self.fir_coeff_ram.port['a'].din
        fir_coeff_ram_width0 = self.fir_coeff_ram.port['a'].width0
        fir_coeff_ram_width1 = self.fir_coeff_ram.port['a'].width1
        fir_coeff_ram_pipe = self.fir_coeff_ram.port['a'].pipe
        fir_coeff_ram_wmode = self.fir_coeff_ram.port['a'].wmode
        fir_coeff_ram_blk = self.fir_coeff_ram.port['a'].blk
        fir_coeff_ram_wen = self.fir_coeff_ram.port['a'].wen
        fir_coeff_ram_clk = self.fir_coeff_ram.port['a'].clk
        fir_coeff_ram_dout = self.fir_coeff_ram.port['a'].dout
        fir_load_coeff_ram_addr = self.fir_coeff_ram.port['b'].addr
        fir_load_coeff_ram_din = self.fir_coeff_ram.port['b'].din
        fir_load_coeff_ram_width0 = self.fir_coeff_ram.port['b'].width0
        fir_load_coeff_ram_width1 = self.fir_coeff_ram.port['b'].width1
        fir_load_coeff_ram_pipe = self.fir_coeff_ram.port['b'].pipe
        fir_load_coeff_ram_wmode = self.fir_coeff_ram.port['b'].wmode
        fir_load_coeff_ram_blk = self.fir_coeff_ram.port['b'].blk
        fir_load_coeff_ram_wen = self.fir_coeff_ram.port['b'].wen
        fir_load_coeff_ram_clk = self.fir_coeff_ram.port['b'].clk
        fir_load_coeff_ram_dout = self.fir_coeff_ram.port['b'].dout
        fir_delay_line_i_ram_addr = self.fir_delay_line_i_ram.port['a'].addr
        fir_delay_line_i_ram_din = self.fir_delay_line_i_ram.port['a'].din
        fir_delay_line_i_ram_width0 = self.fir_delay_line_i_ram.port['a'].width0
        fir_delay_line_i_ram_width1 = self.fir_delay_line_i_ram.port['a'].width1
        fir_delay_line_i_ram_pipe = self.fir_delay_line_i_ram.port['a'].pipe
        fir_delay_line_i_ram_wmode = self.fir_delay_line_i_ram.port['a'].wmode
        fir_delay_line_i_ram_blk = self.fir_delay_line_i_ram.port['a'].blk
        fir_delay_line_i_ram_wen = self.fir_delay_line_i_ram.port['a'].wen
        fir_delay_line_i_ram_clk = self.fir_delay_line_i_ram.port['a'].clk
        fir_delay_line_i_ram_dout = self.fir_delay_line_i_ram.port['a'].dout
        fir_delay_line_q_ram_addr = self.fir_delay_line_q_ram.port['a'].addr
        fir_delay_line_q_ram_din = self.fir_delay_line_q_ram.port['a'].din
        fir_delay_line_q_ram_width0 = self.fir_delay_line_q_ram.port['a'].width0
        fir_delay_line_q_ram_width1 = self.fir_delay_line_q_ram.port['a'].width1
        fir_delay_line_q_ram_pipe = self.fir_delay_line_q_ram.port['a'].pipe
        fir_delay_line_q_ram_wmode = self.fir_delay_line_q_ram.port['a'].wmode
        fir_delay_line_q_ram_blk = self.fir_delay_line_q_ram.port['a'].blk
        fir_delay_line_q_ram_wen = self.fir_delay_line_q_ram.port['a'].wen
        fir_delay_line_q_ram_clk = self.fir_delay_line_q_ram.port['a'].clk
        fir_delay_line_q_ram_dout = self.fir_delay_line_q_ram.port['a'].dout

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
                    tx_fifo_wack=tx_fifo_wack,
                    tx_fifo_dvld=tx_fifo_dvld,
                    tx_fifo_overflow=tx_fifo_overflow,
                    tx_fifo_underflow=tx_fifo_underflow,
                    tx_fifo_rdcnt=tx_fifo_rdcnt,
                    tx_fifo_wrcnt=tx_fifo_wrcnt,
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
                    rx_fifo_wack=rx_fifo_wack,
                    rx_fifo_dvld=rx_fifo_dvld,
                    rx_fifo_overflow=rx_fifo_overflow,
                    rx_fifo_underflow=rx_fifo_underflow,
                    rx_fifo_rdcnt=rx_fifo_rdcnt,
                    rx_fifo_wrcnt=rx_fifo_wrcnt,
                    fir_coeff_ram_addr=fir_coeff_ram_addr,
                    fir_coeff_ram_din=fir_coeff_ram_din,
                    fir_coeff_ram_width0=fir_coeff_ram_width0,
                    fir_coeff_ram_width1=fir_coeff_ram_width1,
                    fir_coeff_ram_pipe=fir_coeff_ram_pipe,
                    fir_coeff_ram_wmode=fir_coeff_ram_wmode,
                    fir_coeff_ram_blk=fir_coeff_ram_blk,
                    fir_coeff_ram_wen=fir_coeff_ram_wen,
                    fir_coeff_ram_clk=fir_coeff_ram_clk,
                    fir_coeff_ram_dout=fir_coeff_ram_dout,
                    fir_load_coeff_ram_addr=fir_load_coeff_ram_addr,
                    fir_load_coeff_ram_din=fir_load_coeff_ram_din,
                    fir_load_coeff_ram_width0=fir_load_coeff_ram_width0,
                    fir_load_coeff_ram_width1=fir_load_coeff_ram_width1,
                    fir_load_coeff_ram_pipe=fir_load_coeff_ram_pipe,
                    fir_load_coeff_ram_wmode=fir_load_coeff_ram_wmode,
                    fir_load_coeff_ram_blk=fir_load_coeff_ram_blk,
                    fir_load_coeff_ram_wen=fir_load_coeff_ram_wen,
                    fir_load_coeff_ram_clk=fir_load_coeff_ram_clk,
                    fir_load_coeff_ram_dout=fir_load_coeff_ram_dout,
                    fir_delay_line_i_ram_addr=fir_delay_line_i_ram_addr,
                    fir_delay_line_i_ram_din=fir_delay_line_i_ram_din,
                    fir_delay_line_i_ram_width0=fir_delay_line_i_ram_width0,
                    fir_delay_line_i_ram_width1=fir_delay_line_i_ram_width1,
                    fir_delay_line_i_ram_pipe=fir_delay_line_i_ram_pipe,
                    fir_delay_line_i_ram_wmode=fir_delay_line_i_ram_wmode,
                    fir_delay_line_i_ram_blk=fir_delay_line_i_ram_blk,
                    fir_delay_line_i_ram_wen=fir_delay_line_i_ram_wen,
                    fir_delay_line_i_ram_clk=fir_delay_line_i_ram_clk,
                    fir_delay_line_i_ram_dout=fir_delay_line_i_ram_dout,
                    fir_delay_line_q_ram_addr=fir_delay_line_q_ram_addr,
                    fir_delay_line_q_ram_din=fir_delay_line_q_ram_din,
                    fir_delay_line_q_ram_width0=fir_delay_line_q_ram_width0,
                    fir_delay_line_q_ram_width1=fir_delay_line_q_ram_width1,
                    fir_delay_line_q_ram_pipe=fir_delay_line_q_ram_pipe,
                    fir_delay_line_q_ram_wmode=fir_delay_line_q_ram_wmode,
                    fir_delay_line_q_ram_blk=fir_delay_line_q_ram_blk,
                    fir_delay_line_q_ram_wen=fir_delay_line_q_ram_wen,
                    fir_delay_line_q_ram_clk=fir_delay_line_q_ram_clk,
                    fir_delay_line_q_ram_dout=fir_delay_line_q_ram_dout,
        )
        return tx_fifo, rx_fifo, self.fir_coeff_ram.ram, self.fir_delay_line_i_ram.ram, self.fir_delay_line_q_ram.ram, whitebox_test

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
            yield bus.receive(WE_GAIN_ADDR)
            gain_i, gain_q = bus.rdata & 0x3ff, (bus.rdata & 0x03ff0000) >> 16
            assert (gain_i / 2.**9) == 1.  # Default is 1
            assert (gain_q / 2.**9) == 1.  # Default is 1
            gain_word = lambda i: intbv(((intbv(int(i[1]*2.**9))[32:] << 16) & 0x03ff0000) | (intbv(int(i[0]*2.**9))[32:] & 0x3ff))[32:]
            yield bus.transmit(WE_GAIN_ADDR, gain_word((0.75, 1.25)))
            yield bus.receive(WE_GAIN_ADDR)
            gain_i, gain_q = bus.rdata & 0x3ff, (bus.rdata & 0x03ff0000) >> 16
            assert (gain_i / 2.**9) == 0.75
            assert (gain_q / 2.**9) == 1.25
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

class TestTxFifo(unittest.TestCase):
    def test_tx_fifo(self):
        symbols = np.linspace(-(2**15-1), 2**15-1, 1024)
        sequence = [concat(intbv(int(symbols[i]))[16:],
                    intbv(int(symbols[i]))[16:]) for i in range(len(symbols))]
        INTERP = 1
        FIFO_DEPTH = len(symbols)
        quantum = len(symbols) // 4
        bus = Apb3Bus(duration=APB3_DURATION)

        s = WhiteboxSim(bus)

        fifo_args = {'width': 32, 'depth': FIFO_DEPTH}
        whitebox_args = { 'interp': INTERP }
        def test_whitebox_tx_fifo():
            return s.cosim_dut("cosim_whitebox_tx_fifo",
                    fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield whitebox_clear(bus)

            yield bus.transmit(WE_INTERP_ADDR, INTERP)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == INTERP

            # Setup the threshold
            yield bus.transmit(WE_THRESHOLD_ADDR,
                concat(intbv(FIFO_DEPTH - quantum)[16:], intbv(quantum)[16:]))
            yield bus.receive(WE_THRESHOLD_ADDR)
            assert bus.rdata == concat(intbv(FIFO_DEPTH - quantum)[16:], intbv(quantum)[16:])

            # Check the fifo flags
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_SPACE
            assert bus.rdata & WES_AEMPTY
            assert not (bus.rdata & WES_DATA)
            assert not (bus.rdata & WES_AFULL)
            yield bus.receive(WE_AVAILABLE_ADDR)
            assert bus.rdata == FIFO_DEPTH

            while N < FIFO_DEPTH - 1:
                yield bus.transmit(WE_SAMPLE_ADDR, sequence[int(N)])
                N.next = N + 1
                yield bus.receive(WE_AVAILABLE_ADDR)
                assert bus.rdata == FIFO_DEPTH - N

                yield bus.receive(WE_STATUS_ADDR)
                assert bus.rdata & WES_SPACE
                assert bus.rdata & WES_DATA
                if N <= quantum:
                    assert bus.rdata & WES_AEMPTY
                else:
                    assert not (bus.rdata & WES_AEMPTY)
                if N < FIFO_DEPTH - quantum:
                    assert not (bus.rdata & WES_AFULL)
                else:
                    assert bus.rdata & WES_AFULL

            yield bus.transmit(WE_SAMPLE_ADDR, sequence[int(N)])
            N.next = N + 1
            yield bus.receive(WE_AVAILABLE_ADDR)
            assert bus.rdata == 0
            yield bus.receive(WE_STATUS_ADDR)
            assert not (bus.rdata & WES_SPACE)
            assert bus.rdata & WES_DATA
            assert not (bus.rdata & WES_AEMPTY)
            assert bus.rdata & WES_AFULL

            yield bus.receive(WE_RUNS_ADDR)
            assert bus.rdata == 0

            yield bus.transmit(WE_SAMPLE_ADDR, sequence[int(N) - 1])
            N.next = N + 1
            yield bus.receive(WE_RUNS_ADDR)
            assert bus.rdata

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

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_tx_fifo, record_tx=FIFO_DEPTH)
        for j in range(FIFO_DEPTH):
            assert s.tx_i[j] == j and s.tx_q[j] == j

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

        sample_rate = 10.00e6
        freq = 100e3
        n = np.arange(0, CNT)
        x = (np.cos(freq * (2 * pi) * n / sample_rate) * (2**15-1)) + \
            (np.sin(freq * (2 * pi) * n / sample_rate) * (2**15-1)) * 1j;

        fifo_args = { 'width': 32, 'depth': FIFO_DEPTH }
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
                record_tx=CNT, auto_stop=False)

        s.plot_tx("whitebox_halt")
        plt.savefig("test_whitebox_halt.png")

class TestLoop(unittest.TestCase):
    def test_loop(self):
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

        def test_whitebox_loop():
            return s.cosim_dut("cosim_whitebox_loop",
                    fifo_args, whitebox_args)

        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])

            yield bus.reset()
            # Send a clear
            yield whitebox_clear(bus)

            #yield bus.transmit(WE_STATUS_ADDR, WS_LOOPEN)

            #yield bus.receive(WR_STATUS_ADDR)
            #assert bus.rdata & WS_LOOPEN

            #yield bus.transmit(WE_INTERP_ADDR, 1)
            #sample = concat(intbv(1 << 6)[16:], intbv(1 << 6)[16:])
            #yield bus.transmit(WE_SAMPLE_ADDR, sample)
            #yield bus.transmit(WE_STATUS_ADDR, WES_TXSTOP)
            #yield bus.transmit(WE_STATUS_ADDR, WES_TXEN)
            #
            #yield bus.receive(WE_STATUS_ADDR)
            #assert bus.rdata & WS_LOOPEN

            #yield bus.transmit(WR_DECIM_ADDR, 1)
            #yield bus.transmit(WR_STATUS_ADDR, WRS_RXSTOP)
            #yield bus.transmit(WR_STATUS_ADDR, WRS_RXEN)
            #yield bus.receive(WR_STATUS_ADDR)
            #while not bus.rdata & WRS_DATA:
            #    yield bus.delay(1)
            #    yield bus.receive(WR_STATUS_ADDR)
            ##assert not (bus.rdata & WRS_RXEN)

            #yield bus.receive(WR_SAMPLE_ADDR)
            #print bus.rdata

            #yield bus.receive(WR_SAMPLE_ADDR)
            #assert bus.rdata == 0xdeadbeef

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_loop, sample_rate=sample_rate)

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

            while True:
                yield bus.delay(1)

            #raise StopSimulation

        s.simulate(stimulus, test_whitebox_cic, sample_rate=output_sample_rate,
                record_tx=CNT, auto_stop=True)

        s.plot_tx("whitebox_cic")
        plt.savefig("test_whitebox_cic.png")

class TestPipeSamples(unittest.TestCase):
    def test_pipe_samples(self):
        bus = Apb3Bus(duration=APB3_DURATION)
        BULK_SIZE=4 #
        FIFO_DEPTH=BULK_SIZE*4
        AFVAL = BULK_SIZE*3 #
        AEVAL = BULK_SIZE*2 #
        SAMPLES_TO_SIMULATE=FIFO_DEPTH << 6
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

            while True:
                yield bus.delay(40)

            raise StopSimulation

        s.simulate(stimulus, test_whitebox_pipe_samples, record_tx=SAMPLES_TO_SIMULATE, auto_stop=True)
        s.plot_tx("whitebox_pipe")
        plt.savefig("test_whitebox_pipe.png")

class WhiteboxImpulseResponseTestCase(unittest.TestCase):
    def setUp(self):
        self.bus = Apb3Bus(duration=APB3_DURATION)
        self.s = WhiteboxSim(self.bus)

        self.fifo_args = { 'width': 32, 'depth': self.fifo_depth }
        self.whitebox_args = { 'interp': self.interp }

    def simulate(self, dut):
        @instance
        def stimulus():
            N = Signal(intbv(0)[32:])
            s = self.s
            bus = self.bus

            yield bus.reset()
            # Send a clear
            yield whitebox_clear(bus)

            # Turn on and off the fir filter
            yield bus.receive(WE_STATUS_ADDR)
            assert not bus.rdata & WS_FIREN
            yield bus.transmit(WE_STATUS_ADDR, bus.rdata | WS_FIREN)
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WS_FIREN
            yield bus.transmit(WE_STATUS_ADDR, bus.rdata & ~WS_FIREN)
            yield bus.receive(WE_STATUS_ADDR)
            assert not bus.rdata & WS_FIREN

            # Set the threshold
            afval = intbv(self.fifo_depth - self.bulk_size)[16:]
            aeval = intbv(self.bulk_size)[16:]
            yield bus.transmit(WE_THRESHOLD_ADDR, concat(afval, aeval))

            if hasattr(self, 'taps') and self.taps:
                yield bus.transmit(W_FIR_ADDR, len(self.taps) | WF_ACCESS_COEFFS)
                for t in self.taps:
                    yield bus.transmit(0, intbv(t << 3)[32:])

                yield bus.receive(W_FIR_ADDR)
                assert bus.rdata == len(self.taps)

                yield bus.transmit(W_FIR_ADDR, len(self.taps) | WF_ACCESS_COEFFS)

                for t in self.taps:
                    yield bus.receive(0)
                    assert bus.rdata.signed() == t << 3

            # Check the fifo flags
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_SPACE
            assert not (bus.rdata & WES_DATA)

            if hasattr(self, 'shift'):
                interp = concat(intbv(self.shift)[16:],
                        intbv(self.interp)[16:])
            else:
                interp = intbv(self.interp)[32:]
            yield bus.transmit(WE_INTERP_ADDR, interp)
            yield bus.receive(WE_INTERP_ADDR)
            assert bus.rdata == interp

            def quadrature_bit_vector(N):
                return \
                intbv(int(self.x[int(N)].real), min=-2**15, max=2**15), \
                intbv(int(self.x[int(N)].imag), min=-2**15, max=2**15)

            ## Insert some samples
            for j in range(self.bulk_size):
                i, q = quadrature_bit_vector(N)
                yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                N.next = N + 1
                yield delay(1)

            ## Now start transmitting
            yield bus.transmit(WE_STATUS_ADDR, self.status | WES_TXEN)
            yield bus.receive(WE_STATUS_ADDR)
            assert bus.rdata & WES_TXEN

            ## Insert some more samples
            while len(self.n) - N > self.bulk_size - 1:
                ## Make sure there were no overruns or underruns
                yield bus.receive(WE_RUNS_ADDR)
                assert bus.rdata == 0

                ## Wait for space
                yield bus.receive(WE_STATUS_ADDR)
                while bus.rdata & WES_AFULL:
                    yield bus.delay(2)
                    yield bus.receive(WE_STATUS_ADDR)

                for j in range(self.bulk_size):
                    i, q = quadrature_bit_vector(N)
                    yield bus.transmit(WE_SAMPLE_ADDR, concat(q, i))
                    N.next = N + 1

            ## Insert remaining samples
            while N < len(self.n)-1:
                ## Wait for space
                yield bus.receive(WE_STATUS_ADDR)
                while not (bus.rdata & WES_SPACE):
                    yield bus.delay(2)
                    yield bus.receive(WE_STATUS_ADDR)

                i, q = quadrature_bit_vector(N)
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

        if hasattr(self, 'record_tx'):
            record_tx = self.record_tx
        else:
            record_tx = self.interp * self.cnt
        self.s.simulate(stimulus, dut, sample_rate=self.sample_rate,
                record_tx=record_tx, auto_stop=False)

class TestUpsamplerImpulseResponse(WhiteboxImpulseResponseTestCase):
    def setUp(self):
        self.apb3_duration = APB3_DURATION
        self.interp = 1
        self.fifo_depth = 64
        self.bulk_size = 16
        self.sample_rate = 6.144e6
        self.freq = 1.7e3 
        self.duration = 4e-6 # 2 mS
        self.status = 0

        self.cnt = ceil(self.sample_rate * self.duration)
        self.n = np.arange(0, self.cnt)
        self.x = np.zeros(self.cnt, dtype=np.complex128)
        self.x[0] = (1 << 14) + 1j * (1 << 14)
        self.y = np.zeros(self.interp*self.cnt, dtype=np.complex128)
        for i in range(self.interp):
            self.y[i] = (1 << 7) + 1j * (1 << 7)
        WhiteboxImpulseResponseTestCase.setUp(self)
        
    def test_whitebox_upsampler_impulse_response(self):
        def test_whitebox_upsampler_impulse_response():
            return self.s.cosim_dut("cosim_whitebox_upsampler_impulse_response",
                    self.fifo_args, self.whitebox_args)

        self.simulate(test_whitebox_upsampler_impulse_response)

        self.s.plot_tx("whitebox_upsampler_impulse_response", decim=self.interp)
        plt.savefig("test_whitebox_upsampler_impulse_response.png")
        assert (self.s.tx() == self.y).all()

class TestUpsampler3xImpulseResponse(WhiteboxImpulseResponseTestCase):
    def setUp(self):
        self.apb3_duration = APB3_DURATION
        self.interp = 3
        self.fifo_depth = 64
        self.bulk_size = 16
        self.sample_rate = 6.144e6
        self.freq = 1.7e3 
        self.duration = 4e-6 # 2 mS
        self.status = 0

        self.cnt = ceil(self.sample_rate * self.duration)
        self.n = np.arange(0, self.cnt)
        self.x = np.zeros(self.cnt, dtype=np.complex128)
        self.x[0] = (1 << 14) + 1j * (1 << 14)
        self.y = np.zeros(self.interp*self.cnt, dtype=np.complex128)
        for i in range(self.interp):
            self.y[i] = (1 << 7) + 1j * (1 << 7)
        WhiteboxImpulseResponseTestCase.setUp(self)
        
    def test_whitebox_upsampler_3x_impulse_response(self):
        def test_whitebox_upsampler_3x_impulse_response():
            return self.s.cosim_dut("cosim_whitebox_upsampler_3x_impulse_response",
                    self.fifo_args, self.whitebox_args)

        self.simulate(test_whitebox_upsampler_3x_impulse_response)

        self.s.plot_tx("whitebox_upsampler_3x_impulse_response", decim=self.interp)
        plt.savefig("test_whitebox_upsampler_3x_impulse_response.png")
        assert (self.s.tx() == self.y).all()

class TestFirImpulseResponse(WhiteboxImpulseResponseTestCase):
    def setUp(self):
        self.apb3_duration = APB3_DURATION
        self.fifo_depth = 64
        self.bulk_size = 16
        self.sample_rate = 6.144e6
        self.freq = 1.7e3 
        self.taps = [1, 2, 3, 5, 8, -5, -3, -2, -1]
        self.interp = 16
        self.status = WS_FIREN

        self.cnt = 128
        self.n = np.arange(0, self.cnt)
        self.x = np.zeros(self.cnt, dtype=np.complex128)
        self.x[0] = (1 << 14) + 1j * (1 << 14)
        self.y = np.zeros(self.cnt, dtype=np.complex128)
        for i in range(len(self.taps)):
            self.y[i] = self.taps[i] + 1j * self.taps[i]
        WhiteboxImpulseResponseTestCase.setUp(self)
        
    def test_whitebox_fir_impulse_response(self):
        def test_whitebox_fir_impulse_response():
            return self.s.cosim_dut("cosim_whitebox_fir_impulse_response",
                    self.fifo_args, self.whitebox_args)

        self.simulate(test_whitebox_fir_impulse_response)

        self.s.plot_tx("whitebox_fir_impulse_response", decim=self.interp)
        plt.savefig("test_whitebox_fir_impulse_response.png")
        assert (self.s.tx(decim=self.interp) == self.y).all()

class TestCicImpulseResponse(WhiteboxImpulseResponseTestCase):
    def setUp(self):
        self.apb3_duration = APB3_DURATION
        self.interp = 1
        self.fifo_depth = 64
        self.bulk_size = 16
        self.sample_rate = 6.144e6
        self.freq = 1.7e3 
        self.status = WES_FILTEREN

        self.cnt = 128
        self.n = np.arange(0, self.cnt)
        self.x = np.zeros(self.cnt, dtype=np.complex128)
        self.x[0] = (1 << 14) + 1j * (1 << 14)
        self.y = np.zeros(self.interp*self.cnt, dtype=np.complex128)
        for i in range(self.interp):
            self.y[i] = (1 << 7) + 1j * (1 << 7)
        WhiteboxImpulseResponseTestCase.setUp(self)
        
    def test_whitebox_cic_impulse_response(self):
        def test_whitebox_cic_impulse_response():
            return self.s.cosim_dut("cosim_whitebox_cic_impulse_response",
                    self.fifo_args, self.whitebox_args)

        self.simulate(test_whitebox_cic_impulse_response)

        self.s.plot_tx("whitebox_cic_impulse_response", decim=self.interp)
        plt.savefig("test_whitebox_cic_impulse_response.png")
        assert (self.s.tx() == self.y).all()

class TestCic3xImpulseResponse(WhiteboxImpulseResponseTestCase):
    def setUp(self):
        from duc import cic_shift
        self.apb3_duration = APB3_DURATION
        self.fifo_depth = 64
        self.bulk_size = 16
        self.sample_rate = 6.144e6
        self.freq = 1.7e3 
        self.duration = 4e-6 # 2 mS
        self.status = WES_FILTEREN
        self.coeffs = [1, 4, 10, 16, 19, 16, 10, 4, 1]

        self.interp = 3
        cic_delay = 1
        cic_order = 4
        self.shift = cic_shift(9, 10, self.interp, 4, 1)
        print 'shift', self.shift
        self.cnt = self.record_tx = 32 #ceil(self.sample_rate * self.duration)
        self.n = np.arange(0, self.cnt)
        self.x = np.zeros(self.cnt, dtype=np.complex128)
        self.x[0] = (1 << 14) + 1j * (1 << 14)
        self.y = np.zeros(self.cnt, dtype=np.complex128)
        for i, c in enumerate(self.coeffs):
            self.y[i] = (c << 3) + 1j * (c << 3)
        WhiteboxImpulseResponseTestCase.setUp(self)
        
    def test_whitebox_cic_3x_impulse_response(self):
        def test_whitebox_cic_3x_impulse_response():
            return self.s.cosim_dut("cosim_whitebox_cic_3x_impulse_response",
                    self.fifo_args, self.whitebox_args)

        self.simulate(test_whitebox_cic_3x_impulse_response)

        self.s.plot_tx("whitebox_cic_3x_impulse_response")
        plt.savefig("test_whitebox_cic_3x_impulse_response.png")
        print len(self.s.tx()), len(self.y)
        print self.s.tx()
        print self.y
        assert (self.s.tx() == self.y).all()


if __name__ == '__main__':
    unittest.main()
