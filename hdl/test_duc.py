import math
import unittest
import numpy as np
import matplotlib.pyplot as plt

from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        delay, now, Simulation, StopSimulation, traceSignals

from duc import *
from test_dsp import load_quadrature_short_samples
from test_dsp import figure_continuous_complex
from test_dsp import figure_discrete_quadrature
from test_dsp import DSPSim

#class TestSequenceFunctions(unittest.TestCase):
#    def test_pass(self):
#        s = DSPSim(
#                in_sign=int16,
#                out_sign=int16,
#        )
#        def test_pass():
#            pass_through_0 = pass_through(s.clearn, s.clock,
#                s.in_valid, s.in_i, s.in_q,
#                s.out_valid, s.out_i, s.out_q)
#            return pass_through_0
#
#        # t is the range of 0 to 2*pi
#        t = np.arange(0, 2*np.pi, 2*np.pi/512)
#        # Input is complex from range [-1 - 1j] to [1 + 1j]
#        in_c = np.cos(t) + 1j * np.sin(t)
#
#        f_pass_0 = plt.figure("pass_0")
#
#        out_c = s.simulate(f_pass_0, in_c, test_pass)
#        #assert np.array_equal(in_c, out_c)
#        np.testing.assert_array_almost_equal(in_c, out_c, decimal=3)
#
#    def test_interpolator(self):
#        s = DSPSim(
#                in_sign=int16,
#                out_sign=int16,
#        )
#        interp = Signal(intbv(10, min=0, max=2**10))
#
#        def test_interpolator_0():
#            interpolator_0 = interpolator(s.clearn, s.clock,
#                interp,
#                s.in_valid, s.in_i, s.in_q,
#                s.out_valid, s.out_i, s.out_q)
#            return interpolator_0
#
#        # t is the range of 0 to 2*pi
#        in_t = np.arange(0, 2*np.pi, 2*np.pi/512)
#        # Input is complex from range [-1 - 1j] to [1 + 1j]
#        in_c = np.cos(in_t) + 1j * np.sin(in_t)
#
#        f_interpolator_0 = plt.figure("interpolator_0")
#
#        f_in = figure_continuous_complex("interpolator_0_in", 222,
#                    f_interpolator_0, in_t, in_c)
#
#        out_c = s.simulate(f_interpolator_0, in_c, test_interpolator_0, interp=interp)
#        out_t = np.arange(0, 2*np.pi, 2*np.pi/out_c.shape[0])
#
#        assert in_t.shape[0] * interp == out_t.shape[0]
#
#        f_out = figure_continuous_complex("interpolator_0_out", 223,
#                    f_interpolator_0, out_t, out_c)

class TestCicImpulse(unittest.TestCase):
    def test_cic_impulse(self):
        cic_order = 3
        cic_delay = 1
        interp = Signal(intbv(4, min=0, max=2**10))

        in_sign = Signature("in", True, bits=2)

        out_sign = Signature("in", True, bits=4)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        cic_0_signals = None
        def test_cic_impulse():
            cic_0 = cic(s.clearn, s.clock, s.input, s.output,
                    interp,
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
        print cic_0_signals
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
        interp = Signal(intbv(20, min=0, max=2**10))
        decim = Signal(intbv(1, min=0, max=2**10))

        in_sign = Signature("in", True, bits=8)
        out_sign = Signature("out", True, bits=8)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        def test_cic_sin():
            cic_0 = cic(s.clearn, s.clock, s.input, s.output,
                    interp,
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
        cic_order = 3
        interp = Signal(intbv(20, min=0, max=2**10))
        decim = Signal(intbv(1, min=0, max=2**10))

        in_sign = Signature("in", True, bits=16)
        out_sign = Signature("out", True, bits=10)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        def test_cic_interpolate_20():
            truncated = Signature("truncated", True, bits=8)
            truncator_0 = truncator(s.clearn, s.clock, s.input, truncated)
            cic_0 = cic(s.clearn, s.clock, truncated, s.output, #cic_0_out,
                    interp,
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


if __name__ == '__main__':
    unittest.main()
