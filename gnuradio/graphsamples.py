from pylab import *
import struct
import json

def plot_samples(*args, **options):
    fig = figure()
    t = arange(options['numsamples'])

    fs = open(options['filename'])
    js = json.load(fs)
    s = [i[0] for i in js]
    
    plot(t, s)

def plot_gnuradio_samples(*args, **options):
    fig = figure()
    struct_fmt = 'hh'
    struct_len = struct.calcsize(struct_fmt)
    struct_unpack = struct.Struct(struct_fmt).unpack_from
    si = []
    sq = []
    if options['filename'] == '-':
        options['filename'] = '/dev/stdin'
    with open(options['filename']) as fs:
        if 'offset' in options:
            i = 0
            while i < options['offset']:
                data = fs.read(struct_len)
                i = i + 1
        while (len(si) < options['numsamples']) if 'numsamples' in options else True:
            data = fs.read(struct_len)
            if data == '': break
            try:
                s = struct_unpack(data)
            except:
                print "weird error"
                pass
            else:
                si.append(s[0])
                sq.append(s[1])

    n = arange(len(si))
    s = np.array(si) + 1j * np.array(sq)

    if options['frequency_domain_plot']:
        Y = np.fft.fft(s)
        frq = np.fft.fftfreq(len(Y), 1 / options['sample_rate'])
        power_S = np.abs(Y) ** 2
        power_db = -10 * np.log10(power_S / np.max(power_S))
        plot(frq[0:len(frq)/2], power_db[0:len(frq)/2], 'b')
        plot(frq[len(frq)/2+1:], power_db[len(frq)/2+1:], 'b')
        ylim(120, 0)
        show()

    if options['time_domain_plot']:
        plot(n, s.real, 'b')
        plot(n, s.imag, 'r')
        show()

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Process and view a raw signal')
    parser.add_argument('-n', '--numsamples', default=1024)
    parser.add_argument('-o', '--offset', default=0)
    parser.add_argument('-r', '--rate', dest='sample_rate', default=10e6)
    parser.add_argument('--fft', dest='frequency_domain_plot', action='store_true', default=False)
    parser.add_argument('--time', dest='time_domain_plot', action='store_true', default=False)
    parser.add_argument('filename', default='-')
    args = parser.parse_args()
    options = vars(args)
    plot_gnuradio_samples(**options)

if __name__ == '__main__':
    main()
