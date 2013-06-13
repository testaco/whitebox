from pylab import *
import json

def plot_samples(*args, **options):
    fig = figure()
    t = arange(options['numsamples'])

    fs = open(options['filename'])
    js = json.load(fs)
    s = [i[0] for i in js]
    
    plot(t, s)
