import math
import unittest
import numpy as np
import matplotlib.pyplot as plt

from myhdl import \
        Signal, ResetSignal, intbv, modbv, enum, concat, \
        instance, always, always_comb, always_seq, \
        delay, now, Simulation, StopSimulation, traceSignals

from duc import *

INTERP=625

SYSCLK_DURATION = int(1e9 / 20e6)

    
def load_quadrature_short_samples(filename, **options):
    import struct
    struct_fmt = 'hh'
    struct_len = struct.calcsize(struct_fmt)
    struct_unpack = struct.Struct(struct_fmt).unpack_from
    si = []
    sq = []
    with open(filename) as fs:
        if 'offset' in options:
            i = 0
            while i < options['offset']:
                data = fs.read(struct_len)
                i = i + 1
        while (len(si) < options['numsamples']) if 'numsamples' in options else True:
            data = fs.read(struct_len)
            if not data: break
            s = struct_unpack(data)
            si.append(s[0])
            sq.append(s[1])
    final_i = np.array(si, dtype=np.int16)
    final_q = np.array(sq, dtype=np.int16)
    return final_i, final_q

def figure_continuous_complex(title, axes, f_parent, t, c):
    f = f_parent.add_subplot(axes, title=title)
    plt.plot(t, c.real, 'b-', label='real', figure=f)
    plt.plot(t, c.imag, 'r-', label='imag', figure=f)
    plt.axis([t[0], t[-1], -1, 1], figure=f)
    #plt.legend()
    return f

def figure_discrete_quadrature(title, axes, f_parent, signature, n, i, q):
    min_without_nan = lambda x: min([i for i in x if i is not np.nan])
    max_without_nan = lambda x: max([i for i in x if i is not np.nan])
    percent = float(max_without_nan(i) - min_without_nan(i)) / float(signature.max - signature.min) * 100.0
    t = title + ' (%.2f)' % percent
    f = f_parent.add_subplot(*axes, title=t)
    plt.stem(n, i, linefmt='b-', markerfmt='b.', basefmt='b|')
    plt.stem(n, q, linefmt='r-', markerfmt='r.', basefmt='r|')
    #plt.axis([n[0], n[-1], signature.min, signature.max], figure=f)
    plt.axis([n[0], n[-1],
        min(min_without_nan(i), min_without_nan(q)),
        max(max_without_nan(i), max_without_nan(q))],
        figure=f)
    #plt.legend()
    return f

class DSPSim(object):
    def __init__(self, in_sign, out_sign):
        self.clock = Signal(bool(0))
        self.clearn = ResetSignal(1, 0, async=False)
        self.input = in_sign
        self.output = out_sign
        self.recording = []
        self.results_i = []
        self.results_q = []

    def read(self):
        if self.output.valid:
            self.results_i.append(int(self.output.myhdl(self.output.i)))
            self.results_q.append(int(self.output.myhdl(self.output.q)))

    def feed(self, i_sample, q_sample, interp=1):
        self.input.i.next = i_sample
        self.input.q.next = q_sample
        self.input.valid.next = True

        self.clock.next = 1
        yield delay(SYSCLK_DURATION // 2)
        self.clock.next = 0
        self.read()
        yield delay(SYSCLK_DURATION // 2)

        self.input.i.next = 0
        self.input.q.next = 0
        self.input.valid.next = False

        for i in range(interp-1):
            self.clock.next = 1
            yield delay(SYSCLK_DURATION // 2)
            self.clock.next = 0
            self.read()
            yield delay(SYSCLK_DURATION // 2)


    def delay(self, count):
        for i in range(count):
            self.clock.next = 1
            yield delay(SYSCLK_DURATION // 2)
            self.clock.next = 0
            yield delay(SYSCLK_DURATION // 2)

    def simulate_quadrature(self, in_i, in_q, dspflow, **kwargs):
        interp = kwargs.get('interp', 1)
        @instance
        def stimulus():
            self.t = 0

            self.clearn.next = self.clearn.active
            yield self.delay(1)
            self.clearn.next = not self.clearn.active

            while self.t < len(in_i):
                yield self.feed(self.input.myhdl(in_i[self.t]),
                        self.input.myhdl(in_q[self.t]), interp)
                self.t = self.t + 1
            self.input.valid.next = False
            yield self.delay(1)
            while self.output.valid:
                self.read()
                yield self.delay(1)
            raise StopSimulation

        # Show the input
        #f_chain_in = figure_discrete_quadrature("chain in", 223, f, self.input, in_n, in_i, in_q)

        traced = traceSignals(dspflow)

        recordings = [s.record(self.clearn, self.clock) for s in self.recording]

        s = Simulation(stimulus, recordings, traced)
        s.run()

        final_i, final_q = (np.array(self.results_i, dtype=self.output.numpy_dtype()),
                np.array(self.results_q, dtype=self.output.numpy_dtype()))
        assert len(final_i) == len(final_q)
        final_n = np.arange(final_i.shape[0])

        #f_chain_out = figure_discrete_quadrature("chain out", 224, f, self.output, final_n, final_i, final_q)
        return final_i, final_q

    def simulate(self, in_c, dspflow, **kwargs):
        # n, for the DSP is an a-range over the sample size
        in_n = np.arange(in_c.shape[0])
        # Convert the input to two signed shorts
        in_i = np.array((self.input.max-1) * in_c.real, dtype=self.input.numpy_dtype())
        in_q = np.array((self.input.max-1) * in_c.imag, dtype=self.input.numpy_dtype())

        final_i, final_q = self.simulate_quadrature(in_i, in_q, dspflow, **kwargs)

        out_c = (final_i / (self.output.max-1.0)) + 1.0j * (final_q / (self.output.max-1.0))

        return out_c



    def record(self, sign):
        self.recording.append(sign)

    def plot_chain(self, name):
        f_parent = plt.figure(name)
        plt.title(name)

        n = 1
        while n*n < len(self.recording):
            n = n + 1
        axes = [(n, n, i) for i in range (1, n * n + 1)]
        axes.reverse()

        for r in self.recording:
            n = np.arange(len(r.samples_i))
            f = figure_discrete_quadrature(r.name, axes.pop(), f_parent, r, n, 
                    r.samples_i, r.samples_q)

        

class TestSequenceFunctions(unittest.TestCase):
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

    def __test_cic_impulse(self):
        cic_order = 3
        cic_delay = 1
        interp = Signal(intbv(4, min=0, max=2**10))

        in_sign = Signature("in", True, bits=2)

        #cic_gain = ((cic_delay * interp) ** cic_order)/interp
        cic_gain = cic_total_gain(interp, cic_delay, cic_order)

        out_sign = in_sign.copy_with_gain("out", cic_gain)
        #out_sign = Signature("out", True, bits=13)
        #out_sign = Signature("out", True, bits=24)

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

        print in_t.shape, out_t.shape
        #assert in_t.shape[0] * interp == out_t.shape[0]

        f_cic_impulse = plt.figure("cic_impulse")
        plt.title("cic_impulse")

        f_in = figure_continuous_complex("in", 211, f_cic_impulse, in_t, in_c)
        f_out = figure_continuous_complex("out", 212, f_cic_impulse, out_t, out_c*int(interp))
        s.plot_chain("cic_impulse_debug")

        plt.show()

    def __test_cic_sin(self):
        cic_delay = 1
        cic_order = 3
        interp = Signal(intbv(20, min=0, max=2**10))
        decim = Signal(intbv(1, min=0, max=2**10))

        in_sign = Signature("in", True, bits=8)
        cic_gain = cic_total_gain(interp, cic_delay, cic_order)
        #out_sign = in_sign.copy_with_gain("out", cic_gain)
        out_sign = Signature("out", True, bits=8)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        def test_cic_sin():
            cic_0 = cic(s.clearn, s.clock, s.input, s.output,
                    interp, decim,
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

        print in_t.shape, out_t.shape
        #assert in_t.shape[0] * interp == out_t.shape[0]

        f_cic_sin = plt.figure("cic_sin")
        plt.title("cic_sin")

        f_in = figure_continuous_complex("in", 211, f_cic_sin, in_t, in_c)
        f_out = figure_continuous_complex("out", 212, f_cic_sin, out_t, out_c)

        s.plot_chain("")
        plt.show()

    def test_cic_interpolate_400(self):
        cic_delay = 1
        cic_order = 3
        interp = Signal(intbv(400, min=0, max=2**10))
        decim = Signal(intbv(1, min=0, max=2**10))

        in_sign = Signature("in", True, bits=16)
        cic_gain = cic_total_gain(interp, cic_delay, cic_order)
        #out_sign = in_sign.copy_with_gain("out", cic_gain)
        out_sign = Signature("out", True, bits=10)

        s = DSPSim(
                in_sign=in_sign,
                out_sign=out_sign,
        )
        def test_cic_interpolate_400():
            truncated = Signature("truncated", True, bits=8)
            truncator_0 = truncator(s.clearn, s.clock, s.input, truncated)
            cic_0 = cic(s.clearn, s.clock, truncated, s.output, #cic_0_out,
                    interp,
                    cic_order=cic_order, cic_delay=cic_delay,
                    sim=None)
            return cic_0, truncator_0

        numsamples = 256
        #fname = '/home/testa/whitebox/build/nbfm_voice.samples'
        fname = '/home/testa/whitebox/hdl/sin.samples'
        in_i, in_q = load_quadrature_short_samples(fname, offset=65012, numsamples=numsamples)
        in_t = np.arange(0, in_i.shape[0])

        try:
            out_i, out_q = s.simulate_quadrature(in_i, in_q, test_cic_interpolate_400, interp=interp)
        except:
            #s.plot_chain("cic_interpolate_400_error")
            #plt.show()
            raise

        out_t = np.arange(0, out_i.shape[0])

        f_cic_interpolate_400 = plt.figure("cic_interpolate_400")
        #plt.title("cic_interpolate_400")

        f_in = figure_discrete_quadrature("in", (2, 1, 1), f_cic_interpolate_400, in_sign, in_t, in_i, in_q)
        f_out = figure_discrete_quadrature("out", (2, 1, 2), f_cic_interpolate_400, out_sign, out_t, out_i, out_q)

        s.plot_chain("")
        plt.show()

if __name__ == '__main__':
    unittest.main()
