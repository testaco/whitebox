Features
--------

.. raw:: html
    
    <div class="row"><div class="col-md-6">

**Hardware**

* Quadrature receiver from 100MHz to 1000MHz
* Quadrature transmitter from 100MHz to 1000MHz
* Half-duplex
* 1MHz bandwidth
* 10 MS/s, 10 bit ADC & DAC
* 16MB SDRAM, 16MB Flash
* 3.3V low power operation
* 100mA sleep state, 500mA transceiving

**Interfaces**

* UART over USB
* 10/100 Ethernet
* ARM & FlashPro JTAG
* 1 I2C, 1 SPI, 1 UART, 1 ADC, 16 GPIO

.. raw:: html
    
    </div><div class="col-md-6">

**Software**

* Host PC Generation and Simulation
* uClinux 2.6.xx + Busybox from Emcraft Systems
* Digital Signal Processing Chain for RF
* Platform and Character Drivers for RF Board
* Userspace control libraries and examples
* GNURadio drivers


**Applications**

* Analog/digital multimode radio
* Software deifined radio
* Battery powered baseband modems
* Portable, mobile and base station terminals
* Satellite communications
* Amateur radio
* Police and commercial radio


.. raw:: html
    
    </div></div>

General Description
*******************

The Whitebox Framework is a suite of tools to design, simulate, and generate
efficient, low power, signal processing systems
for embedded Linux computers based on SoC FPGA's.
It includes an embeddable software radio with a small
footprint and power profile, like a smartphone cellular radio architecture,
but aim'ed at VHF/UHF operation.

At the core is a customizable System on a Chip
with both an ARM Cortex-M3 running uClinux / Flash based FPGA;
paired with a frequency agile radio frontend.
Drivers handle transporting data from userspace Linux to antenna with zero memory copy.
Supports GNURadio over UDP in Peripheral mode for rapid development.

The module is targeted at battery operated instrumentation.
In particular is has low static power dissapation (<100mA).
Augment the embedded system
with a full suite of embedded communication controllers
including I2C, SPI, UART, and GPIOs.
Attach it to an Application SoC
and make a Smart Software Radio with Android.


Functional Block Diagram
************************

.. image:: img/functional_block_diagram.png

**Signals**

1. Input Voltage (VIN).  Battery, car, or USB powered.
2. Serial I/O (USB).  Busybox shell accessable over USB.  I2C, SPI, UART, GPIOs available for expansion.
3. JTAG Debugging (JTAG).  Debug both the ARM core and FPGA via JTAG.
4. 10/100 Ethernet (ETH). Supports TFTP boot, NFS mount, and GNURadio over UDP.
5. Receiver (RX SMA).  Receive antenna SMA.
6. Transmitter (TX SMA).  Transmit antenna SMA.
7. 10MHz Reference (REFIN).  External 10MHz reference input SMA.

