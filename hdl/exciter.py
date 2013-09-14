"""
Exciter
=======

The exciter works like so:
    1. Sensors, like the 1-bit key, or the 10-bit adc from microphone
       even Memory!  Perhaps always memory?  It always comes at a bitrate.
    2. Encoders, that take data and convert them into symbols at a baud rate.
    3. Right now, everything is quadrature data, I[n] & Q[n].  These are 16 bit fixed
       point numbers.
    4. The mode outputs, always I[n] & Q[n] are fed to the DAC, which has analog errors.
    5. A filter is applied to the outputted I[n] & Q[n] analog signals.
    6. The analog radio:
        1. Quadrature Modulates the signal with the carrier f_if
            I[n]*cos(2*pi*f_if) + Q[n]*sin(2*pi*f_if)
        2. It then filters the output
        3. It then Image-Reject upconverts with the f_rf
    7. The frontend then:
        1. goes through a matching balun network
        2. into a filter
        3. then an amplifier
        4. then an antenna

"""
from math import sin, pi
import struct

from myhdl import Signal, delay, always, always_comb, always_seq, now, \
                  Simulation, intbv, instance, enum, traceSignals, \
                  toVerilog, StopSimulation, concat

from fifo import fifo as FIFO
from apb3_utils import Apb3Bus
from dds import dds as DDS

APB3_DURATION = int(1e9 / 10e6)
SYSCLK_DURATION = int(1e9 / 20e6)

class OverrunError(Exception):
    pass

class UnderrunError(Exception):
    pass

BULK_SIZE=64
INTERP=200
SAMPLES_TO_SIMULATE=500
FCW=int(50e6)

def exciter(
        resetn,
        system_clock,
        pclk,
        paddr,
        psel,
        penable,
        pwrite,
        pwdata,
        pready,
        prdata,
        pslverr,
        dac_clock,
        dac_data,
        status_led):
    """The exciter.

    :param resetn: Reset
    :param system_clock: The main fabric clock
    :param pclk: The system bus clock
    :param paddr: The bus assdress
    :param psel: The bus slave select
    :param penable: The bus slave enable line
    :param pwrite: The bus read/write flag
    :param pwdata: The bus write data
    :param pready: The bus slave ready signal
    :param prdata: The bus read data
    :param pslverr: The bus slave error flag
    :param dac_clock: The DAC clock
    :param dac_data: The DAC data
    :param status_led: Output pin for exciter status
    """
    clearn = Signal(bool(1))

    ####### FIFO ############
    # Read
    re = Signal(bool(False))
    rclk = system_clock
    Q = Signal(intbv(0)[32:])
    Q.driven = 'wire'

    # Write
    we = Signal(bool(False))
    wclk = pclk
    data = Signal(intbv(0)[32:])

    # Threshold
    full = Signal(bool(False))
    full.driven = 'wire'
    afull = Signal(bool(False))
    afull.driven = 'wire'
    empty = Signal(bool(False))
    empty.driven = 'wire'
    aempty = Signal(bool(False))
    aempty.driven = 'wire'

    fifo_resetn = Signal(bool(1))

    fifo_args = fifo_resetn, re, rclk, Q, we, wclk, data, full, afull, \
        empty, aempty
    fifo = FIFO(*fifo_args,
        width=32,
        depth=BULK_SIZE*4,
        threshold=BULK_SIZE*1)


    ######### RESAMPLER ###########
    interp = Signal(intbv(1)[32:])
    interp_counter = Signal(intbv(0)[32:])

    sample_i = Signal(intbv(0)[10:])
    sample_q = Signal(intbv(0)[10:])

    samples_count = Signal(intbv(0)[32:])

    ########## STATE MACHINE ######
    state_t = enum('IDLE', 'WRITE_SAMPLE', 'READ_FLAG', 'DONE',)
    state = Signal(state_t.IDLE)

    ########## DDS ################
    use_dds = Signal(bool(False))
    dds_sample = Signal(intbv(0, min=-2**9, max=2**9))
    fcw = Signal(intbv(1)[32:])
    dds_args = resetn, dac_clock, dds_sample, fcw
    phase = Signal(bool(False))
    dds = DDS(*dds_args)

    ############ TX EN ###########
    txen = Signal(bool(0))
    underrun = Signal(bool(False))
    overrun = Signal(bool(False))

    @always_seq(pclk.posedge, reset=resetn)
    def state_machine():
        fifo_resetn.next = resetn and clearn
        if state == state_t.IDLE:
            if penable and psel:
                if paddr[8:] == 0x00:
                    if pwrite:
                        if full:
                            overrun.next = True
                            state.next = state_t.DONE
                        else:
                            state.next = state_t.WRITE_SAMPLE
                            pready.next = 0
                            data.next = pwdata

                elif paddr[8:] == 0x04:
                    if pwrite:
                        print 'hi', pwdata
                        txen.next = pwdata[0]
                        status_led.next = pwdata[0]
                        pready.next = 0
                        state.next = state_t.DONE
                        if pwdata[5]:
                            overrun.next = False
                            clearn.next = 0
                        use_dds.next = pwdata[6]
                    else:
                        prdata.next = concat(use_dds, clearn, afull, aempty, underrun, overrun, txen)
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == 0x08:
                    if pwrite:
                        interp.next = pwdata
                        pready.next = 0
                        state.next = state_t.DONE
                    else:
                        prdata.next = interp
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == 0x0c:
                    if pwrite:
                        fcw.next = pwdata
                        pready.next = 0
                        state.next = state_t.DONE
                    else:
                        prdata.next = fcw
                        pready.next = 0
                        state.next = state_t.READ_FLAG

                elif paddr[8:] == 0x10:
                    if not pwrite:
                        prdata.next = samples_count
                    state.next = state_t.READ_FLAG


        elif state == state_t.WRITE_SAMPLE:
            we.next = 1
            state.next = state_t.DONE

        elif state == state_t.READ_FLAG:
            state.next = state_t.DONE

        elif state == state_t.DONE:
            we.next = 0
            pready.next = 1
            clearn.next = 1
            state.next = state_t.IDLE

    @always(system_clock.posedge)
    def duc():
        if not clearn:
            dac_data.next = 0
            dac_clock.next = 0
            phase.next = 0
            re.next = False
            sample_i.next = 0
            sample_q.next = 0
            interp_counter.next = 0
            samples_count.next = 0
            underrun.next = False
        if txen:
            # DAC output
            dac_data.next = sample_i if phase else sample_q
            dac_clock.next = not phase
            phase.next = not phase

            if use_dds:
                sample_i.next = concat(not dds_sample[9], dds_sample[9:])
                sample_q.next = concat(not dds_sample[9], dds_sample[9:])
            else:
                # Resample interp times.
                if phase:
                    interp_counter.next = interp_counter + 1
                    if interp_counter == interp:
                        interp_counter.next = 0
                        re.next = True
                        samples_count.next = samples_count + 1

                # Get a new sample, converting from 2's complement to offset binary
                # for the DAC.
                if re:
                    sample_i.next = concat(not Q[15], Q[15:6])
                    sample_q.next = concat(not Q[31], Q[31:22])
                    re.next = False

                # Watch for underrun
                if empty:
                    underrun.next = True

    return fifo, state_machine, duc, dds

if __name__ == '__main__':
    bus = Apb3Bus(duration=APB3_DURATION)
    
    sclk = Signal(bool(0))
    dac_clock = Signal(bool(0))
    dac_data = Signal(intbv(0)[10:])
    status_led = Signal(bool(0))

    signals = (bus.presetn,
                sclk,
                bus.pclk,
                bus.paddr,
                bus.psel,
                bus.penable,
                bus.pwrite,
                bus.pwdata,
                bus.pready,
                bus.prdata,
                bus.pslverr,
                dac_clock,
                dac_data,
                status_led)

    @always(delay(SYSCLK_DURATION // 2))
    def stimulus():
        sclk.next = not sclk

    def _sim():
        sresetn = bus.presetn
        bus_pclk = bus.pclk
        bus_paddr = bus.paddr
        bus_psel = bus.psel
        bus_penable = bus.penable
        bus_pwrite = bus.pwrite
        bus_pwdata = bus.pwdata
        bus_pready = bus.pready
        bus_prdata = bus.prdata
        bus_pslverr = bus.pslverr
        N = Signal(intbv(0)[32:])

        e = exciter(*signals)

        @instance
        def __sim():
            yield bus.reset()

            # Send a clear
            yield bus.transmit(0x04, 0x20)

            #yield bus.transmit(0x08, INTERP)
            #yield bus.receive(0x08)
            #assert bus.rdata == INTERP

            ## Insert samples until overrun
            #yield bus.receive(0x04)
            #while not (bus.rdata & 0x02):
            #    x = intbv(int(sin(1000 * (2 * pi) * N / 50000) * 2**15))[16:]
            #    yield bus.transmit(0x00, concat(x, x))
            #    N.next = N + 1
            #    yield bus.receive(0x04)

            ## Now start transmitting
            #yield bus.transmit(0x04, 1)
            #yield bus.receive(0x04)
            #assert bus.rdata & 0x01 == 1

            ## Wait until underrun
            #while not (bus.rdata & 0x04):
            #    yield bus.delay(100*BULK_SIZE)
            #    yield bus.receive(0x04)

            ## Make sure we're both over and underrun
            #assert bus.rdata & 0x04 and bus.rdata & 0x02

            # This is how to start a working transmit of a sin wave at 1KHz

            # Load the samples file
            f = open('/home/testa/whitebox/hdl/sin.samples', 'r')
            struct_fmt = 'I'
            struct_len = struct.calcsize(struct_fmt)
            assert struct_len == 4
            struct_unpack = struct.Struct(struct_fmt).unpack_from
            def read_sample():
                data = f.read(struct_len)
                if not data: raise StopSimulation
                return intbv(struct_unpack(data)[0])[32:]

            # Go in a bit.
            #for i in range(SAMPLES_TO_SIMULATE):
            #    read_sample()


            # Send a clear
            yield bus.transmit(0x04, 0x20)

            N.next = 0
            yield delay(100)

            # Seed the buffer.
            while N < 128:
                yield bus.transmit(0x00, read_sample())
                N.next = N + 1

            # Make sure that we haven't overrun/underrun on seeding the buffer.
            yield bus.receive(0x04)
            assert not (bus.rdata & 0x04) and not (bus.rdata & 0x02)

            yield bus.transmit(0x0c, FCW)
            yield bus.receive(0x0c)
            assert bus.rdata == FCW
            yield bus.transmit(0x08, INTERP)
            yield bus.receive(0x08)
            assert bus.rdata == INTERP

            # Start the transmit
            yield bus.transmit(0x04, 0x41)

            #while N < SAMPLES_TO_SIMULATE:
            #    # First, check the status of the exciter
            #    yield bus.receive(0x04)

            #    if bus.rdata & 0x02:
            #        raise OverrunError
            #    if bus.rdata & 0x04:
            #        raise UnderrunError

            #    # If we're almost full, wait a little bit for samples to
            #    # clear through.
            #    while bus.rdata & 0x10:
            #        yield bus.delay(BULK_SIZE*100)
            #        yield bus.receive(0x04)

            #    # Things don't look good if we're almost empty...
            #    if bus.rdata & 0x08:
            #        print '\n\n************warning almost underrun'

            #    # Insert BULK_SIZE samples
            #    for j in range(BULK_SIZE):
            #        yield bus.transmit(0x00, read_sample())
            #        N.next = N + 1

            #    # Wait a little bit.
            #    yield bus.delay(10*1024)

            ### Now, wait for all of the samples to finish sending
            #bus.receive(0x10)
            #while bus.rdata < SAMPLES_TO_SIMULATE:
            #    yield bus.delay(10*1024)
            #    yield bus.receive(0x10)
            yield bus.delay(1000)

            # And turn off the transmitter
            yield bus.transmit(0x04, 0)
            yield bus.receive(0x04)
            assert bus.rdata & 0x01 == 0

            raise StopSimulation


        return __sim, e

    s = Simulation(stimulus, traceSignals(_sim))
    s.run()

    toVerilog(exciter, *signals)
