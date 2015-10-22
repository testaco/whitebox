import math
import numpy as np
from matplotlib import pyplot as plt
from scipy import signal

audio_rates = (48000,)
modem_rates = (50000,)

def resampler_rate(in_rate, out_rate):
    for i in range(1, 1000):
        for d in range(1, 1000):
            if (in_rate * i % d == 0 and in_rate * i / d == out_rate):
                return i, d
    raise Exception, 'Impossible'

def design_audio_resampler(in_rate, out_rate):
    i, d = resampler_rate(in_rate, out_rate)
    filter_rate = in_rate * i
    nyq_rate = filter_rate / 2.
    fractional_bw = (min(in_rate, out_rate) / 2.) / nyq_rate
    fractional_bw = (1. / min(i, d))

    print 'resample', in_rate, 'to', out_rate, 'is', i, ':', d,
    print '    fractional_bw', fractional_bw

    #num_taps = 256
    atten = 60.
    num_taps, beta = signal.kaiserord(atten, fractional_bw)

    taps = signal.firwin(num_taps, fractional_bw, window=('kaiser', beta))

    # Append zeros onto the taps to make polyphase decomposition easier
    while len(taps) % i != 0:
        taps = np.append(taps, [0.])
    num_taps = len(taps)

    #taps = signal.remez(num_taps, [0, fractional_bw, fractional_bw, .5], [1, 0])
    # Gain due to zero stuffing
    taps *= i
    # Attenuation due to filter max gain
    taps /= sum(taps)

    print 'taps per sample', float(len(taps)) / i

    plt.figure("filter-taps")
    plt.plot(np.arange(num_taps), taps)
    plt.xlim(0, num_taps - 1)
    w, h = signal.freqz(taps)
    plt.figure("filter-response")
    plt.plot((w / np.pi) * nyq_rate, 20 * np.log10(abs(h)))
    x = np.zeros(len(taps))
    x[0] = 1.0
    filtered_x = signal.lfilter(taps, 1.0, x)
    assert (filtered_x == taps).all()
    return i, d, taps

def graph_audio_resampler(in_rate, out_rate, i, d, taps, x, y):
    # These operations must be done with a FPU to maintain precision
    l, m = float(i), float(d)

    assert ((len(y) - (len(x) * l / m)) <= 1)

    N = len(taps)
    delay = int(.5 * (N - 1.)) * ((l - 1.) / m)
    delay_start = int(math.ceil((N - 1.) * l / m))
    print 'delay', delay, delay_start

    plt.figure("time-in")
    n = np.arange(len(x))
    plt.plot(n, x)
    plt.xlim(0, len(n) - 1)
    plt.ylim(-1, 1)
    plt.show(block=False)

    # And plot the results
    plt.figure("time-out")
    n = np.arange(len(y))
    #plt.plot(n - delay, y)
    plt.plot(n[delay_start:] - delay, y[delay_start:])
    plt.xlim(delay_start, len(n) - 1)
    plt.ylim(-1, 1)
    plt.show(block=False)

    plt.figure("fft-in")
    frq = np.fft.fftfreq(len(x), 1. / in_rate)
    X = np.fft.fft(x)
    power_X = np.abs(X) ** 2
    power_db = 10 * np.log10(power_X / np.max(power_X))
    plt.plot(frq[0:len(frq)/2], power_db[0:len(frq)/2])
    plt.ylim(-60, 0)
    plt.xlim(0, in_rate / 2)
    plt.show(block=False)

    plt.figure("fft-out")
    frq = np.fft.fftfreq(len(y[delay_start:]), 1. / out_rate)
    Y = np.fft.fft(y[delay_start:])
    power_Y = np.abs(Y) ** 2
    power_db = 10 * np.log10(power_Y / np.max(power_Y))
    plt.plot(frq[0:len(frq)/2], power_db[0:len(frq)/2])
    plt.ylim(-60, 0)
    plt.xlim(0, out_rate / 2)
    plt.show()

def inefficient_resampler(i, d, taps, x):
    # Interpolate by zero stuffing
    x_interp = np.zeros(i * len(x))
    n_interp = np.arange(len(x_interp))
    for k, xk in enumerate(x):
        x_interp[i*k] = xk

    # And then low pass filtering; don't forget the gain!
    x_filtered = signal.lfilter(taps, 1.0, x_interp)
    n_filtered = n_interp

    # Finally, decimate by dropping samples
    x_decimated = x_filtered[::d]
    return x_decimated

def prototype_audio_resampler(in_rate, out_rate, x):
    # Design filter
    i, d, taps = design_audio_resampler(in_rate, out_rate)

    x_decimated = inefficient_resampler(i, d, taps, x)

    n_decimated = np.arange(len(x_decimated))

    # Make sure we got the right number of samples out
    #print "lfilter", len(x_decimated)

    # And plot the results
    graph_audio_resampler(in_rate, out_rate, i, d, taps, x, x_decimated)

def fir_kernel(a, a_start, a_incr, b, b_start, b_incr, count):
    c = 0.

    an = a_start
    bn = b_start

    #print 'fir kernel start x[%d::%d] * h[%d::%d]' % (a_start, a_incr, b_start, b_incr)
    for i in range(count):
        #print '   taps[%d] * x[%d]' % (bn, an)
        c += a[an] * b[bn]
        an += a_incr
        an %= len(a)
        #if an < 0:
        #    an = len(a) + an
        bn += b_incr
        bn %= len(b)
        #if bn < 0:
        #    bn = len(b) + bn

    return c

# This is the magic sauce.  A rational resampler, that does the minimum
# ammount of computation.  The next step is to move it to Verilog
def polyfir_rational(interp, decim, taps, x):
    #polyfir_rational_new(interp, decim, taps, x)
    z = np.zeros(int(float(len(x)) * interp / decim))
    # Polyphase decomposition, so split up the taps into
    # interp phases.
    polyphase_count = int(float(len(taps)) / interp)
    phase = 0
    x_off = 0
    h_off = 0

    # Sample memory, interp long
    y = np.zeros(interp)
    y_off = 0
    xx = np.nditer(x)
    y[y_off] = x[0]

    for i in range(len(z)):
        assert(x_off == int(float(i) * decim / interp)), '%d %d' % (x_off, int(float(i) * decim / interp))

        z[i] = fir_kernel(
            # The input signal
            y,
            # The input sample index relates to the output sample
            # index; for each output sample increment by decim;
            # keep the same input sample for interp output samples.
            y_off,
            # Move back one sample for each product in the sum
            # That means we need polyphase_count
            -1,
            # The coefficients
            taps,
            # The starting coefficient; move up by decim each
            # output sample, and wrap on the number of polyphases
            # which is interp.
            h_off,
            # Step through the coefficients by interp.
            interp,
            # Only have to compute one phase of coefficients.
            polyphase_count)

        # Switch to the new polyphase decim away each output sample,
        # and wrap on the number of polyphases.
        h_off = (h_off + decim) % interp

        # For each output sample, increase the phase by decim.  If
        # the phase overflows past interp, then move to the next
        # input sample and wrap the phase.
        phase += decim
        #if phase >= 2 * interp:
        #    x_off += 2
        #    y_off = (y_off + 2) % interp
        #    y[y_off] = xx.next()
        #    phase = phase - interp
        print 'go'
        while phase >= interp:
            print '  doing it', phase, interp
            x_off += 1
            y_off = (y_off + 1) % interp
            y[y_off] = xx.next()
            phase = phase - interp
        print '  not doing it', phase, interp
    return z

def polyfir_interp(interp, taps, x):
    return polyfir_rational(interp, 1, taps, x)

    #y = np.zeros(interp * len(x))

    #k_count = int(math.floor(len(taps) / float(interp)))
    #k_incr = interp
    #n_incr = interp

    ## x changes once every 'interp' clock steps.
    #for m in range(len(x)):
    #    # But you still compute y once every clock cycle.
    #    for i in range(interp):
    #        k = i % (interp)
    #        n = m
    #        #print 'computing sample', i
    #        for j in range(k_count):
    #            #print '   taps[%d] * x[%d] +=> y[%d]' % (k, n, i)
    #            y[m * interp + i] += taps[k] * x[n]
    #            k = (k + k_incr) % len(taps)
    #            n -= n_incr

    ## Gain due to (imaginary) zero stuffing
    ##y *= interp

    #return y

def test_polyfir_i(interp, h):
    x = np.ones(len(h)*4)

    # Compare the polyphase decomposition;
    # The sum of the polyphase components show up after the the filter
    # reaches steady state.
    E = []
    for k in range(interp):
        E.append([h[n * interp + k] for n in range(len(h)) if n * interp + k < len(h)])
    print 'polyphase expansion inerp=%d len(h)=%d len(E)=%d,%d' % (interp,
            len(h), len(E[0]), len(E[-1]))

    y = polyfir_interp(interp, h, x)
    assert len(y) == len(x) * interp
    print '    x=', x
    print '    h=', h
    print '    E=', E
    print 'sum(E)=', [ sum(e) for e in E ]
    print '    y=', y
    for i in range(interp):
        assert abs(float(y[-len(h) + i]) - float(sum(E[i]))) < 1e-16

# Test polyfir_interp
def test_polyfir_interp():
    h = np.arange(20)
    x = np.zeros(20)
    x[0] = 1.
    #assert (polyfir_interp(1, h, x) == h).all()
    test_polyfir_i(1, h)
    test_polyfir_i(2, h)
    test_polyfir_i(10, h)

    return 
    for a in audio_rates:
        for m in modem_rates:
            i, d, h = design_audio_resampler(a, m)
            test_polyfir_i(i, h)
            i, d, h = design_audio_resampler(m, a)
            test_polyfir_i(i, h)

def test_polyfir_d(decim, h):
    #x = np.ones(len(h)*1)
    x = np.ones(len(h)*4)

    # Compare the polyphase decomposition;
    # The sum of the polyphase components show up after the the filter
    # reaches steady state.
    E = []
    for k in range(decim):
        E.append([h[n * decim + k] for n in range(len(h)) if n * decim + k < len(h)])
    #print 'polyphase expansion decim=%d len(h)=%d len(E)=%d,%d' % (decim,
    #        len(h), len(E[0]), len(E[-1]))

    y = polyfir_rational(1, decim, h, x)
    #print 'h=', E
    #print 'y=', y, sum(y)
    print '    x=', x
    print '    h=', h
    print 'sum(h)=', sum(h)
    print '    E=', E
    print 'sum(E)=', [ sum(e) for e in E ]
    print '    y=', y
    assert len(y) == len(x) / decim
    #print '    y=', y
    for i in range(decim):
        assert abs(float(y[-1]) - float(sum(h))) < 1e-16

def test_polyfir_r(interp, decim, h):
    E = []
    for k in range(decim):
        E.append([h[n * decim + k] for n in range(len(h)) if n * decim + k < len(h)])
    x = np.zeros(len(h) * 4)
    x[0] = 1.
    y = polyfir_rational(interp, decim, h, x)
    print '    x=', x
    print '    h=', h
    print 'sum(h)=', sum(h)
    print '    E=', E
    print 'sum(E)=', [ sum(e) for e in E ]
    print '    y=', y
    print 'sum(y)=', sum(y)
    assert abs(len(y) - len(x) * interp / decim) <= 1
    #print '    y=', y


def test_polyfir_rational():
    h = np.arange(20)
    x = np.zeros(20)
    x[0] = 1.
    #assert (polyfir_rational(1, 1, h, x) == h).all()

    test_polyfir_d(1, h)
    test_polyfir_d(2, h)
    test_polyfir_d(10, h)

    i, d, h = design_audio_resampler(48e3, 64e3)
    test_polyfir_r(i, d, h)

def prototype_audio_resampler_poly(in_rate, out_rate, x):
    # Design filter
    i, d, taps = design_audio_resampler(in_rate, out_rate)

    # No need to zero stuff. but still need the gain
    x_filtered = polyfir_rational(i, d, taps, x) * i

    # No need to decimate either, very efficient!
    graph_audio_resampler(in_rate, out_rate, i, d, taps, x, x_filtered)

def audio_source(sample_rate, duration=1):
    count = int(sample_rate * duration)
    # Create band-limited white noise
    taps = signal.firwin(255, [300., 3700.],
            pass_zero=False, nyq=sample_rate/2)
    n = np.arange(count)
    x = np.random.normal(size=count)
    x /= 20**.5
    #x = signal.lfilter(taps, 1.0, x)

    # Add in a tone
    x += np.sin(2 * np.pi * 700 * n / sample_rate)
    x += np.sin(2 * np.pi * 1900 * n / sample_rate)

    # Normalize
    x /= max(abs(x))

    return x

rational_resampler = prototype_audio_resampler_poly

def build_audio_to_modem_resampler():
    for a in audio_rates:
        # Create input signal
        x = audio_source(a)
        for m in modem_rates:
            rational_resampler(a, m, x)

def build_modem_to_audio_resampler():
    for m in modem_rates:
        # Create input signal
        x = audio_source(m)
        for a in audio_rates:
            rational_resampler(m, a, x)


test_polyfir_interp()
test_polyfir_rational()

plt.close('all')

build_modem_to_audio_resampler()
build_audio_to_modem_resampler()
