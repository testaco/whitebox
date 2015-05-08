import math
import unittest
import numpy as np
import matplotlib.pyplot as plt

from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        delay, now, Simulation, StopSimulation, traceSignals

from duc import *
from ddc import cic_decim
from test_dsp import load_quadrature_short_samples
from test_dsp import figure_continuous_complex
from test_dsp import figure_discrete_quadrature
from test_dsp import DSPSim

class TestCicImpulse(unittest.TestCase):
    def test_cic_impulse(self):
        cic_order = 3
        cic_delay = 1
        interp = Signal(intbv(4, min=0, max=2**10))
        shift = Signal(intbv(0, min=0, max=21))

        in_sign = Signature("in", True, bits=2)

        out_sign = Signature("in", True, bits=4)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        cic_0_signals = None
        def test_cic_impulse():
            cic_0 = cic(s.clearn, s.clock, s.input, s.output,
                    interp, shift,
                    cic_order=cic_order, cic_delay=cic_delay,
                    sim=s)

            return cic_0

        ## t is the range of 0 to 2*pi
        in_t = np.arange(0, 10)

        ## Input is impulse
        in_c = 0*in_t + 1j * 0*in_t
        for i in range (1):
            in_c[i] = 1 + 0j

        out_c = s.simulate(in_c, test_cic_impulse, interp=interp)
        print '***', cic_0_signals
        out_t = np.arange(0, out_c.shape[0])

        new_shape = tuple([in_t.shape[i] * interp for i in range(len(in_t.shape))])
        assert out_t.shape == new_shape


        f_cic_impulse = plt.figure("cic_impulse")
        plt.title("cic_impulse")

        f_in = figure_continuous_complex("in", 211, f_cic_impulse, in_t, in_c)
        f_out = figure_continuous_complex("out", 212, f_cic_impulse, out_t, out_c*int(interp))
        s.plot_chain("cic_impulse_debug")

        plt.savefig("cic_impulse.png")

class TestCicSin(unittest.TestCase):
    def test_cic_sin(self):
        cic_delay = 1
        cic_order = 3
        in_sign = Signature("in", True, bits=8)
        out_sign = Signature("out", True, bits=8)

        interp_val = 20
        interp = Signal(intbv(interp_val, min=0, max=2**10))
        shift_val = cic_shift(len(in_sign.i), len(out_sign.i), interp_val,
                cic_order, cic_delay)
        shift = Signal(intbv(shift_val, min=0, max=21))
        decim = Signal(intbv(1, min=0, max=2**10))

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        def test_cic_sin():
            cic_0 = cic(s.clearn, s.clock, s.input, s.output,
                    interp, shift,
                    cic_order=cic_order, cic_delay=cic_delay,
                    sim=s)
            return cic_0

        # t is the range of 0 to 2*pi
        in_t = np.arange(0, 2*np.pi, 2*np.pi/8)
        # Input is complex from range [-1 - 1j] to [1 + 1j]
        in_c = .98 * (np.cos(in_t) + 1j * np.sin(in_t)) + \
               .02 * (np.cos(in_t*2) + 1j * np.sin(in_t*2))

        try:
            out_c = s.simulate(in_c, test_cic_sin, interp=interp)
        except:
            #s.plot_chain("cic_sin_error")
            #plt.show()
            raise

        out_t = np.arange(0, out_c.shape[0])

        new_shape = tuple([in_t.shape[i] * interp for i in range(len(in_t.shape))])
        assert out_t.shape == new_shape

        f_cic_sin = plt.figure("cic_sin")
        plt.title("cic_sin")

        f_in = figure_continuous_complex("in", 211, f_cic_sin, in_t, in_c)
        f_out = figure_continuous_complex("out", 212, f_cic_sin, out_t, out_c)

        s.plot_chain("")
        plt.savefig("cic_sin.png")


class TestCicInterpolate20(unittest.TestCase):
    def test_cic_interpolate_20(self):
        cic_delay = 1
        cic_order = 4

        in_sign = Signature("in", True, bits=16)
        out_sign = Signature("out", True, bits=16)

        interp_val = 20
        interp = Signal(intbv(interp_val, min=0, max=2**10))
        decim = Signal(intbv(1, min=0, max=2**10))
        shift_val = cic_shift(len(in_sign.i), len(out_sign.i), interp_val,
                cic_order, cic_delay)
        shift = Signal(intbv(shift_val, min=0, max=21))

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        def test_cic_interpolate_20():
            truncated = Signature("truncated", True, bits=8)
            truncator_0 = truncator(s.clearn, s.clock, s.input, truncated)
            cic_0 = cic(s.clearn, s.clock, truncated, s.output, #cic_0_out,
                    interp, shift,
                    cic_order=cic_order, cic_delay=cic_delay,
                    sim=None)
            return cic_0, truncator_0

        numsamples = 32
        fname = '/home/testa/whitebox/hdl/sin.samples'
        in_i, in_q = load_quadrature_short_samples(fname, offset=65012, numsamples=numsamples)
        in_t = np.arange(0, in_i.shape[0])

        try:
            out_i, out_q = s.simulate_quadrature(in_i, in_q, test_cic_interpolate_20, interp=interp)
        except:
            #s.plot_chain("cic_interpolate_20_error")
            #plt.show()
            raise

        out_t = np.arange(0, out_i.shape[0])

        new_shape = tuple([in_t.shape[i] * interp for i in range(len(in_t.shape))])
        assert out_t.shape == new_shape

        f_cic_interpolate_20 = plt.figure("cic_interpolate_20")

        f_in = figure_discrete_quadrature("in", (2, 1, 1), f_cic_interpolate_20, in_sign, in_t, in_i, in_q)
        f_out = figure_discrete_quadrature("out", (2, 1, 2), f_cic_interpolate_20, out_sign, out_t, out_i, out_q)

        plt.savefig("cic_interp_20.png")

class TestCicDecimMath(unittest.TestCase):
    def test_cic_decim_math(self):
        from ddc import cic_decim_max_bits, cic_decim_bit_truncation
        # Test from Hogenauer's paper.
        N = 4; M = 1; R = 25; Bin = 16; Bout = 16
        Bmax = cic_decim_max_bits(Bin, R, N, M)
        assert(Bmax == 34)
        expected = [1, 6, 9, 13, 14, 15, 16, 17]
        actual = [cic_decim_bit_truncation(Bin, Bout, R, N, M, j) for j in range(1, 2*N + 1)]
        assert expected == actual

class TestCicDecimImpulse(unittest.TestCase):
    def test_cic_decim_impulse(self):
        cic_order = 4
        cic_delay = 1
        decim = Signal(intbv(8, min=0, max=2**10))
        #shift = Signal(intbv(0, min=0, max=21))

        in_sign = Signature("in", True, bits=16)

        out_sign = Signature("out", True, bits=16)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        cic_0_signals = None
        def test_cic_decim_impulse():
            cic_0 = cic_decim(s.clearn, s.clock, s.input, s.output,
                    decim, #shift,
                    cic_order=cic_order, cic_delay=cic_delay,
                    sim=s)

            return cic_0

        ## t is the range of 0 to 2*pi
        in_t = np.arange(0, cic_order * decim * cic_delay * 100)

        ## Input is impulse
        in_c = 0*in_t + 1j * 0*in_t
        in_c[0] = .01 + 0j

        # t is the range of 0 to 2*pi
        in_t = np.arange(0, 2*np.pi, 2*np.pi/(decim * 10))
        # Input is complex from range [-1 - 1j] to [1 + 1j]
        in_c = .1 * (np.cos(in_t) + 1j * np.sin(in_t))

        print in_c

        out_c = s.simulate(in_c, test_cic_decim_impulse, decim=decim)
        print out_c
        out_t = np.arange(0, out_c.shape[0])

        new_shape = tuple([in_t.shape[i] / decim - 1 for i in range(len(in_t.shape))])

        print out_t.shape, new_shape
        #assert out_t.shape == new_shape


        f_cic_impulse = plt.figure("cic_decim_impulse")
        plt.title("cic_decim_impulse")

        f_in = figure_continuous_complex("in", 211, f_cic_impulse, in_t, in_c)
        f_out = figure_continuous_complex("out", 212, f_cic_impulse, out_t, out_c/int(decim))
        s.plot_chain("cic_decim_impulse_debug")

        plt.savefig("cic_decim_impulse.png")

if __name__ == '__main__':
    unittest.main()
