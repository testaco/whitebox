Digital Signal Processing
=========================

.. automodule:: dsp

The *block diagram* representation of the signal uses symbols to represent
the interconnections of the core parts of a signal processing flow including
mathematical operations and memory.  In the case of RTL, the memory is
Registers, and the Transfer Logic implements
the flow control and mathematical operations.

The input and output of the signal is represented by a ``Signature``,
and this class represents a a stage of the flow diagram.  The signature
tracks the minimum and maximum values allowed, the number of bits required
to store the signal, and methods to transform the signal into
both MyHDL and numpy types.

.. autoclass:: dsp.Signature
    :members: record

A chain of DSP blocks can be simulated with MyHDL.  You apply an input series
of complex samples, and then record the output series.
Convenience functions are offered to show the phase and frequency response
of the system.

.. autoclass:: test_dsp.DSPSim
    :members: simulate

FIFO Buffer
-----------

The FIFO buffer is the building block that produces samples into, and consumes
samples out of the DSP chain.

This implementation cannot be synthesized into Verilog, and is the only module
that has a dependency on the Vendor specific toolset.
That fact could be changed by instead abstracting the SRAM model and build
a synthesizable FIFO on top of that.

.. automodule:: fifo

.. autofunction:: fifo.fifo

Synthesizable Blocks
--------------------

This is a list of synthesizable blocks.

.. autofunction:: dsp.offset_corrector

.. autofunction:: dsp.gain_corrector

.. autofunction:: dsp.binary_offseter

.. autofunction:: dsp.iqmux

.. autofunction:: dsp.iqdemux

.. autofunction:: dsp.truncator

.. autofunction:: duc.upsampler

.. autofunction:: ddc.downsampler

.. autofunction:: duc.interpolator

.. autofunction:: dsp.delay_n

.. autofunction:: dsp.comb

.. autofunction:: dsp.accumulator

.. autofunction:: duc.cic

.. autofunction:: duc.interleaver

Digital Up Converter
--------------------

The Digital Up Converter takes a low-sample rate stream and up-converts
it to a high sample rate, while conditioning it for an Analog Quadrature
Modulator.

It consists of a synchronizer, a FIFO consumer, and a DSP chain.
The synchronizer safely transfers flags from the system clock domain.

The consumer reads samples off the FIFO queue and feeds them into the DSP chain.

The DSP chain can work in both rectangular and polar coordinates.
This means that you can:

1. Send i and q samples directly and have them digitally up-converted, or
2. Send phase offset and frequency changes to modulate a Direct Digital Synthesizer

.. automodule:: duc

.. autofunction:: duc.duc

Digital Down Converter
----------------------

.. automodule:: ddc

Direct Digital Synthesizer
--------------------------

.. automodule:: dds

.. autofunction:: dds.dds

Here is the LUT generated for 10-bit resolution, 256 samples, and 80% scale.

.. plot::

    import matplotlib.pyplot as plt
    import numpy as np
    import dds
    i, q = dds.dds_lut(10, 256, 0.8)
    n = np.arange(256)
    plt.plot(n, i, n, q)
