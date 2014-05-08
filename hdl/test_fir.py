from numpy import cos, sin, pi, absolute, arange, int32, array, array_equal
from scipy.signal import kaiserord, lfilter, firwin, freqz, remez, periodogram
from pylab import figure, clf, plot, xlabel, ylabel, xlim, ylim, title, grid, axes, show, semilogy, subplot, savefig
from math import log
from myhdl import Signal, intbv, instance
import unittest

from dsp import Signature
from test_dsp import DSPSim, figure_discrete_quadrature
from fir import fir
from ram import Ram, Ram2

COEFF_SHIFT = 12

class FirSim(DSPSim):
    def input_signal(self):
        #------------------------------------------------
        # Create a signal for demonstration.
        #------------------------------------------------

        # Width of the channel
        self.channel_width = 2e3 + self.width/2

        bfo_hz = self.cutoff_hz# - audio_min

        print "bfo_hz is ", bfo_hz

        nsamples = self.sample_rate / 25
        t = self.t = arange(nsamples) / self.sample_rate

        carriers_hz = [1e3 + 75*i for i in range(15)]
        carrier_a = [1. for i in range(15)]
        carrier_phase = [(pi/2)*i for i in range(15)]
        carrier_a[7] = 2.
        print carriers_hz, carrier_a
        a = self.a = sum([(a / 16.) * cos(2*pi*c*t + p) for a, p, c in zip(carrier_a, carrier_phase, carriers_hz)])  # Baseband waveform

        self.re = a * cos(2*pi*bfo_hz*t)  # Mix
        self.im = a * sin(2*pi*bfo_hz*t)  # Mix
        
        return self.re + 1j * self.im

    def design(self, sample_rate, cutoff_hz, width, ripple_db):
        #------------------------------------------------
        # Create a FIR filter and apply it to x.
        #------------------------------------------------

        self.sample_rate = sample_rate
        nyq_rate = self.nyq_rate = sample_rate / 2.0
        self.cutoff_hz = cutoff_hz
        self.width = width

        # The desired width of the transition from pass to stop,
        # relative to the Nyquist rate.  We'll design the filter
        # with a 5 Hz transition width.
        #width = (audio_min * 2)/nyq_rate

        # The desired attenuation in the stop band, in dB.
        #ripple_db = 40.0

        # Compute the order and Kaiser parameter for the FIR filter.
        N, beta = kaiserord(ripple_db, width/nyq_rate)

        # Use firwin with a Kaiser window to create a lowpass FIR filter.
        taps = firwin(N, cutoff_hz/nyq_rate, window=('kaiser', beta))

        self.unweighted_taps = array(taps)

        non_zero_coeffs = len([i for i in taps if int(i*10000000) != 0])
        print 'cost', non_zero_coeffs, 'taps', N

        #Scaling factor
        input_bit_width = self.input.bits
        accumulator_bit_width = 9 + 18 + 1 #2 * input_bit_width + 1
        sum_interval = (-2**(accumulator_bit_width-1), 2**(accumulator_bit_width-1) - 1)
        product_interval = (sum_interval[0] / non_zero_coeffs, sum_interval[1] / non_zero_coeffs)
        input_interval = (-2**(input_bit_width-1), 2**(input_bit_width-1) - 1)
        tap_interval = (- (product_interval[0] / input_interval[0]), product_interval[1] / input_interval[1])
        tap_max = max(abs(tap_interval[0]), tap_interval[1])
        print sum_interval, product_interval, input_interval, tap_interval

        fudge_factor = 2**(log(non_zero_coeffs, 2)-1)
        self.taps = (taps * tap_max * fudge_factor).astype(int32)
        import numpy as np
        assert np.all([abs(t) < 2**17-1 for t in taps])

        print 'taps', [t for t in self.taps]
        return self.taps

    def verify_design(self):
        # Use lfilter to filter x with the FIR filter.
        filtered_x = lfilter(self.taps, 1.0, self.re) / 2**self.input.bits

        #------------------------------------------------
        # Plot the FIR filter coefficients.
        #------------------------------------------------

        figure(1)
        plot(self.unweighted_taps, 'bo-', linewidth=2)
        N = len(self.taps)
        title('Filter Coefficients (%d taps)' % N)
        grid(True)

        savefig('coeffs.png')

        #------------------------------------------------
        # Plot the magnitude response of the filter.
        #------------------------------------------------

        figure(2)
        clf()
        w, h = freqz(self.unweighted_taps, worN=8000)
        #plot((w/pi)*nyq_rate, absolute(h), linewidth=2)
        semilogy((w/pi)*self.nyq_rate, absolute(h), 'b')
        xlabel('Frequency (Hz)')
        ylabel('Gain')
        title('Frequency Response')
        ylim(-0.05, 1.05)
        grid(True)


        # Upper inset plot.
        ax1 = axes([0.42, 0.7, .45, .15])
        plot((w/pi)*self.nyq_rate, absolute(h), linewidth=2)
        xlim(self.cutoff_hz - self.channel_width, self.cutoff_hz)
        # TODO: draw a line at bfo_hz = cutoff_hz - audio_min
        ylim(0.5, 1.01)
        grid(True)

        # Lower inset plot
        ax2 = axes([0.42, 0.15, .45, .15])
        plot((w/pi)*self.nyq_rate, absolute(h), linewidth=2)
        xlim(self.cutoff_hz + self.width/2, self.cutoff_hz + self.channel_width)
        ylim(10e-4, 10e-3)
        grid(True)

        savefig('response.png')

        #------------------------------------------------
        # Plot the original and filtered signals.
        #------------------------------------------------

        # The phase delay of the filtered signal.
        delay = 0.5 * (N-1) / self.sample_rate

        figure(3)
        subplot(211)
        # Plot the original signal.
        plot(self.t, self.re)
        # Plot the filtered signal, shifted to compensate for the phase delay.
        plot(self.t-delay, filtered_x, 'r-')
        # Plot just the "good" part of the filtered signal.  The first N-1
        # samples are "corrupted" by the initial conditions.
        plot(self.t[N-1:]-delay, filtered_x[N-1:], 'g', linewidth=2)
        xlabel('t')
        grid(True)

        subplot(212)
        f, Pxx_den = periodogram(self.re, self.sample_rate)
        semilogy(f, Pxx_den)
        f, Pxx_den = periodogram(filtered_x[N-1:], self.sample_rate)
        semilogy(f, Pxx_den, 'g', linewidth=2)
        xlabel('frequency [Hz]')
        ylabel('Power')
        ylim(10e-12, 1)
        grid(True)


        savefig('output.png')


        # Plot the audio

        figure(4)
        subplot(211)
        plot(self.t, self.a)
        xlabel('t')
        grid(True)
        subplot(212)
        f, Pxx_den = periodogram(self.a, self.sample_rate)
        semilogy(f, Pxx_den)
        ylim(10e-12, 1)
        xlabel('frequency [Hz]')
        ylabel('Power')
        grid(True)

        savefig('input.png')

        show()

class TestFirImpulse(unittest.TestCase):
    def test_fir_impulse(self):
        in_sign = Signature("in", True, bits=9)
        out_sign = Signature("out", True, bits=9)

        s = FirSim(in_sign=in_sign, out_sign=out_sign)
        s.design(48e3, 6e3, 1e3, 40.0)
        taps = (4 << COEFF_SHIFT, 3 << COEFF_SHIFT, 2 << COEFF_SHIFT, 1 << COEFF_SHIFT)
        coeff_ram = Ram2(s.clearn, s.clock, s.clock, data=taps)
        delay_line_i_ram = Ram(s.clearn, s.clock, s.clock)
        delay_line_q_ram = Ram(s.clearn, s.clock, s.clock)
        bypass = Signal(bool(0))
        bank1 = Signal(bool(0))
        bank0 = Signal(bool(0))
        N = Signal(intbv(4, min=0, max=2**7-1))

        def test_fir_impulse():
            load_coeff_ram_addr = coeff_ram.port['b'].addr
            load_coeff_ram_blk = coeff_ram.port['b'].blk
            load_coeff_ram_wen = coeff_ram.port['b'].wen
            fir_0 = fir(s.clearn, s.clock, s.input, s.output,
                    coeff_ram.port['a'].addr,
                    coeff_ram.port['a'].din[0],
                    coeff_ram.port['a'].din[1],
                    coeff_ram.port['a'].blk,
                    coeff_ram.port['a'].wen,
                    coeff_ram.port['a'].dout[0],
                    coeff_ram.port['a'].dout[1],
                    delay_line_i_ram.port['a'].addr,
                    delay_line_i_ram.port['a'].din,
                    delay_line_i_ram.port['a'].blk,
                    delay_line_i_ram.port['a'].wen,
                    delay_line_i_ram.port['a'].dout,
                    delay_line_q_ram.port['a'].addr,
                    delay_line_q_ram.port['a'].din,
                    delay_line_q_ram.port['a'].blk,
                    delay_line_q_ram.port['a'].wen,
                    delay_line_q_ram.port['a'].dout,
                    bypass, bank1, bank0, N,
                    sim=s)

            return fir_0, coeff_ram.rama, coeff_ram.ramb, delay_line_i_ram.ram, delay_line_q_ram.ram

        in_t = arange(0, 8)

        in_c = 0*in_t + 1j * 0*in_t
        in_i = 0*in_t
        in_q = 0*in_t
        in_i[0] = 1 << 5
        in_i[4] = 1 << 5

        out_i, out_q = s.simulate_quadrature(in_i, in_q, test_fir_impulse, interp=128)
        out_t = arange(0, out_i.shape[0])

        new_shape = tuple([in_t.shape[i] for i in range(len(in_t.shape))])
        assert out_t.shape == new_shape
        print out_i
        assert array_equal(out_i, [4, 3, 2, 1, 4, 3, 2, 1])

class TestFirBypass(unittest.TestCase):
    def test_fir_bypass(self):
        in_sign = Signature("in", True, bits=9)
        out_sign = Signature("out", True, bits=9)

        s = FirSim(in_sign=in_sign, out_sign=out_sign)
        s.design(48e3, 6e3, 1e3, 40.0)
        s.input_signal()

        taps = (4 << COEFF_SHIFT, 3 << COEFF_SHIFT, 2 << COEFF_SHIFT, 1 << COEFF_SHIFT)
        coeff_ram = Ram2(s.clearn, s.clock, s.clock, data=taps)
        delay_line_i_ram = Ram(s.clearn, s.clock, s.clock)
        delay_line_q_ram = Ram(s.clearn, s.clock, s.clock)
        bypass = Signal(bool(1))
        bank1 = Signal(bool(0))
        bank0 = Signal(bool(0))
        N = Signal(intbv(4, min=0, max=2**7-1))

        def test_fir_bypass():
            load_coeff_ram_addr = coeff_ram.port['b'].addr
            load_coeff_ram_blk = coeff_ram.port['b'].blk
            load_coeff_ram_wen = coeff_ram.port['b'].wen
            fir_0 = fir(s.clearn, s.clock, s.input, s.output,
                    coeff_ram.port['a'].addr,
                    coeff_ram.port['a'].din[0],
                    coeff_ram.port['a'].din[1],
                    coeff_ram.port['a'].blk,
                    coeff_ram.port['a'].wen,
                    coeff_ram.port['a'].dout[0],
                    coeff_ram.port['a'].dout[1],
                    delay_line_i_ram.port['a'].addr,
                    delay_line_i_ram.port['a'].din,
                    delay_line_i_ram.port['a'].blk,
                    delay_line_i_ram.port['a'].wen,
                    delay_line_i_ram.port['a'].dout,
                    delay_line_q_ram.port['a'].addr,
                    delay_line_q_ram.port['a'].din,
                    delay_line_q_ram.port['a'].blk,
                    delay_line_q_ram.port['a'].wen,
                    delay_line_q_ram.port['a'].dout,
                    bypass, bank1, bank0, N,
                    sim=s)

            return fir_0, coeff_ram.ram, delay_line_i_ram.ram, delay_line_q_ram.ram

        in_t = arange(0, 8)

        in_c = 0*in_t + 1j * 0*in_t
        in_i = 0*in_t
        in_q = 0*in_t
        in_i[0] = 1 << COEFF_SHIFT
        in_i[4] = 1 << COEFF_SHIFT

        out_i, out_q = s.simulate_quadrature(in_i, in_q, test_fir_bypass, interp=128)
        out_t = arange(0, out_i.shape[0])

        new_shape = tuple([in_t.shape[i] for i in range(len(in_t.shape))])
        assert out_t.shape == new_shape
        assert array_equal(out_i >> 5, array([1, 0, 0, 0, 1, 0, 0, 0]))

class TestFirDesign(unittest.TestCase):
    def test_fir_design(self):
        in_sign = Signature("in", True, bits=9)
        out_sign = Signature("out", True, bits=9)

        s = FirSim(in_sign=in_sign, out_sign=out_sign)
        taps = [int(t) for t in s.design(48e3, 12e3, 1e3, 40.0)]

        in_c = s.input_signal()
        in_t = arange(len(in_c))
        in_i = [int(i.real * (2**8-1)) for i in in_c]
        in_q = [int(i.imag * (2**8-1)) for i in in_c]

        s.verify_design()

        coeff_ram = Ram2(s.clearn, s.clock, s.clock, data=taps)
        delay_line_i_ram = Ram(s.clearn, s.clock, s.clock)
        delay_line_q_ram = Ram(s.clearn, s.clock, s.clock)
        bypass = Signal(bool(0))
        bank1 = Signal(bool(0))
        bank0 = Signal(bool(0))
        N = Signal(intbv(len(taps), min=0, max=2**7-1))

        def test_fir_design():
            load_coeff_ram_addr = coeff_ram.port['b'].addr
            load_coeff_ram_blk = coeff_ram.port['b'].blk
            load_coeff_ram_wen = coeff_ram.port['b'].wen
            fir_0 = fir(s.clearn, s.clock, s.input, s.output,
                    coeff_ram.port['a'].addr,
                    coeff_ram.port['a'].din[0],
                    coeff_ram.port['a'].din[1],
                    coeff_ram.port['a'].blk,
                    coeff_ram.port['a'].wen,
                    coeff_ram.port['a'].dout[0],
                    coeff_ram.port['a'].dout[1],
                    delay_line_i_ram.port['a'].addr,
                    delay_line_i_ram.port['a'].din,
                    delay_line_i_ram.port['a'].blk,
                    delay_line_i_ram.port['a'].wen,
                    delay_line_i_ram.port['a'].dout,
                    delay_line_q_ram.port['a'].addr,
                    delay_line_q_ram.port['a'].din,
                    delay_line_q_ram.port['a'].blk,
                    delay_line_q_ram.port['a'].wen,
                    delay_line_q_ram.port['a'].dout,
                    bypass, bank1, bank0, N,
                    sim=s)

            return fir_0, coeff_ram.rama, coeff_ram.ramb, delay_line_i_ram.ram, delay_line_q_ram.ram

        out_i, out_q = s.simulate_quadrature(in_i, in_q, test_fir_design, interp=128)
        out_t = arange(0, out_i.shape[0])

        new_shape = tuple([in_t.shape[i] for i in range(len(in_t.shape))])
        assert out_t.shape == new_shape
        f = figure("fir_output")
        title("fir filter output")
        f_out = figure_discrete_quadrature('FIR Filter Output', (1, 1, 1), f, s.input, out_t, out_i / (2.**8-1), out_q / (2.**8-1))
        show()


if __name__ == '__main__':
    unittest.main()
