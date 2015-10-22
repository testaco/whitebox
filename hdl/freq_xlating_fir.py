import numpy as np
from matplotlib import pyplot as plt
from scipy import signal

from modem import graph_modulation

def iq_source(duration, tone_freq, input_rate):
    count = int(input_rate * duration)
    print count

    # Create wite noise
    n = np.arange(count)
    z = np.random.normal(size=count) + 1j * np.random.normal(size=count)

    # Add in a tone
    z += (np.cos(2 * np.pi * tone_freq * n / input_rate) + 1j * np.sin(2 * np.pi * tone_freq * n / input_rate))

    # Normalize
    z /= max(abs(z))

    return z

def frequency_xlating_fir(z, center_freq, input_rate, decim, bw=50e3):
    # First use firwin
    taps = signal.firwin(64, bw, nyq=input_rate / 2)

    n = np.arange(len(z))
    nco = np.cos(2*np.pi * center_freq * n / input_rate) + \
        -1j * np.sin(2 * np.pi * center_freq * n / input_rate)

    zz = z * nco

    zfinal = signal.lfilter(taps, 1.0, zz)

    return zfinal[::decim]


def graph_iq_transform(name, z, zprime, input_rate, output_rate):
    plt.figure("%s-input" % name)
    plt.plot(np.arange(len(z)), z.real, 'b')
    plt.plot(np.arange(len(z)), z.imag, 'r')
    plt.xlim(0, len(z) - 1)
    plt.ylim(-1, 1)
    plt.show(block=False)

    X = np.fft.fft(z)
    power_x = np.abs(X) ** 2
    Y = np.fft.fft(zprime)
    power_y = np.abs(Y) ** 2

    max_power = max(max(power_x), max(power_y))

    plt.figure("%s-input-fft" % name)
    power_x_db = 10 * np.log10(power_x / max_power)
    frq_x = np.fft.fftfreq(len(z), 1. / input_rate)
    plt.plot(frq_x[0:len(frq_x)/2], power_x_db[0:len(frq_x)/2])
    plt.ylim(-120, 0)
    plt.show(block=False)

    plt.figure("%s-output" % name)
    plt.plot(np.arange(len(zprime)), zprime.real, 'b')
    plt.plot(np.arange(len(zprime)), zprime.imag, 'r')
    plt.xlim(0, len(zprime) - 1)
    plt.ylim(-1, 1)
    plt.show(block=False)

    plt.figure("%s-output-fft" % name)
    power_y_db = 10 * np.log10(power_y / max_power)
    frq_y = np.fft.fftfreq(len(zprime), 1. / output_rate)
    plt.plot(frq_y[0:len(frq_y)/2], power_y_db[0:len(frq_y)/2], 'b')
    plt.plot(frq_y[len(frq_y)/2+1:], power_y_db[len(frq_y)/2+1:], 'b')
    plt.ylim(-120, 0)
    plt.show()

def test(duration, center_freq, input_rate, output_rate):
    z = iq_source(duration, center_freq + 1e3, input_rate)
    #plt.plot(np.arange(len(z)), z)
    #plt.show()

    decim = int(input_rate / output_rate)
    assert decim * output_rate == input_rate
    bb = frequency_xlating_fir(z, center_freq, input_rate, decim)
    #plt.plot(np.arange(len(bb)), bb)
    #plt.show()
    graph_iq_transform('test', z, bb, input_rate, output_rate)

if __name__ == '__main__':
    test(10e-3, 2e6, 10e6, 200e3)
