from math import cos, pi, ceil
from myhdl import Signal, delay, always, always_comb, now, Simulation, intbv, instance

import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt


# Reference: http://www.daniweb.com/software-development/python/code/227432/a-floating-point-range-genertaor
def frange(start, stop=None, step=1.0, delta=0.0000001):
    if stop == None:
        stop = start
        start = 0.0
    if step <= 0:
        while start > (stop + delta):
            yield start
            start += step
    else:
        while start < (stop - delta):
            yield start
            start += step


# Constant system clock frequency
SYSTEM_CLOCK_FREQ = 10e6
# Constant signal output frequency.  Mark (1) 1200Hz, Space (0) 2200Hz
SIGNAL_FREQ = 1200
# Constant resolution... noise... amount of memory... something like that?

SYSTEM_CLOCK_PERIOD_IN_NS = int(1.0 / SYSTEM_CLOCK_FREQ * 1e9)
print "SYSTEM_CLOCK_PERIOD_IN_NS: ", SYSTEM_CLOCK_PERIOD_IN_NS

SIGNAL_PERIOD_IN_NS = int(1.0 / SIGNAL_FREQ * 1e9)
print "SIGNAL_PERIOD_IN_NS: ", SIGNAL_PERIOD_IN_NS

# Derived Constant number of samples
NUM_SAMPLES = 100 # TODO
# Derived Constant number of samples in quadrature phase shift
NUM_SAMPLES_IN_PHASE_SHIFT = NUM_SAMPLES/4
# Derived Constant cycles per sample
CYCLES_PER_SAMPLE = (SIGNAL_PERIOD_IN_NS / SYSTEM_CLOCK_PERIOD_IN_NS) / NUM_SAMPLES
HALF_CLOCK = CYCLES_PER_SAMPLE / 2

print "CYCLES_PER_SAMPLE: ", CYCLES_PER_SAMPLE

# ROM signal samples
RESOLUTION_IN_BITS = 10
HALF = 2 ** (RESOLUTION_IN_BITS - 1)
SAMPLES = tuple([int(ceil(cos(i)*(HALF-1))+HALF) for i in frange(0, 2*pi, step=(2*pi)/NUM_SAMPLES)])

# For testing
i_samples = []
i_samples_times = []
q_samples = []
q_samples_times = []


def signalgenerator(resetn, system_clock, transmit_enable, select, dac_data, dac_clock, SAMPLES):
    i_index = Signal(intbv(0, 0, NUM_SAMPLES))
    q_index = Signal(intbv(0, 0, NUM_SAMPLES))
    in_phase = Signal(bool(0))
    cycles_this_sample = Signal(intbv(0, 0, CYCLES_PER_SAMPLE))

    @always_comb
    def rom():
        dac_data.next = SAMPLES[i_index if in_phase else q_index]

    @always(system_clock.posedge, resetn.negedge)
    def state_machine():
        if not resetn:
            i_index.next = 0
            q_index.next = NUM_SAMPLES_IN_PHASE_SHIFT
            in_phase.next = 0
            cycles_this_sample.next = 0
            print "resetting"
        elif transmit_enable and select:
            i_index.next = i_index
            q_index.next = q_index
            in_phase.next = in_phase

            if cycles_this_sample == CYCLES_PER_SAMPLE - 1:
                i_index.next = (i_index + 1) % NUM_SAMPLES if not in_phase else i_index
                q_index.next = (q_index + 1) % NUM_SAMPLES if in_phase else q_index
                in_phase.next = not in_phase
                dac_clock.next = not in_phase
                cycles_this_sample.next = 0
            else:
                cycles_this_sample.next = cycles_this_sample + 1

    return state_machine, rom

def test_signalgenerator(resetn, system_clock, transmit_enable, select, dac_data, dac_clock, SAMPLES, period=SYSTEM_CLOCK_PERIOD_IN_NS):
    low_time = int(period / 2)
    high_time = period - low_time
    in_phase2 = Signal(bool(0))

    @instance
    def drive_system_clock():
        resetn.next = 0
        yield delay(SYSTEM_CLOCK_PERIOD_IN_NS*10)
        resetn.next = 1
        while True:
            yield delay(low_time)
            system_clock.next = 1
            yield delay(high_time)
            system_clock.next = 0

    @always(dac_clock.posedge)
    def snoop_i_dac_output():
        i_samples.append(int(dac_data))
        i_samples_times.append(now())

    @always(dac_clock.negedge)
    def snoop_q_dac_output():
        q_samples.append(int(dac_data))
        q_samples_times.append(now())

    return drive_system_clock, snoop_i_dac_output, snoop_q_dac_output, signalgenerator(resetn, system_clock, transmit_enable, select, dac_data, dac_clock, SAMPLES)


def get_signals():
    from myhdl import Signal
    resetn = Signal(bool(1))
    system_clock = Signal(bool(0))
    transmit_enable = Signal(bool(1))
    select = Signal(bool(1))
    dac_data = Signal(intbv(0, 0, 2**RESOLUTION_IN_BITS))
    dac_clock = Signal(bool(0))
    return resetn, system_clock, transmit_enable, select, dac_data, dac_clock, SAMPLES

def main_analyze():
    fig = plt.figure()
    ax = fig.add_subplot(111)
    ax.plot(SAMPLES)
    #ax.ylabel("Amplitude")
    #ax.xlabel("Time (samples")
    #ax.title("Signal Sample")
    fig.savefig('sample_time_domain.png')


def main_simulate():
    inst = test_signalgenerator(*get_signals())
    sim = Simulation(inst)
    #sim.run(SYSTEM_CLOCK_PERIOD_IN_NS * 1000)
    sim.run(.01*1e9)  # run for 1ms
    fig = plt.figure()
    ax = fig.add_subplot(111)
    ax.plot(i_samples_times, i_samples, q_samples_times, q_samples)

    fig.savefig('output.png')

def main_generate():
    from myhdl import toVerilog
    toVerilog(signalgenerator, *get_signals())

if __name__ == '__main__':
    main_analyze()
    main_simulate()
    main_generate()
