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
    with open(options['filename']) as fs:
        if 'offset' in options:
            i = 0
            while i < options['offset']:
                data = fs.read(struct_len)
                i = i + 1
        while (len(ss) < options['numsamples']) if 'numsamples' in options else True:
            data = fs.read(struct_len)
            if not data: break
            s = struct_unpack(data)
            si.append(s[0])
            sq.append(s[1])
    t = arange(len(si))
    plot(t, si)
    plot(t, sq)

if __name__ == '__main__':
    plot_gnuradio_samples(filename=sys.argv[1])
    show()
