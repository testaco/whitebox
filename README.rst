.. image:: https://raw.github.com/testaco/whitebox/master/docs/_images/whitebox-logo.jpg
    :alt: Whitebox Logo
    :align: center

The Smart Software Radio Device
===============================

Welcome to the Whitebox Software Radio Project, a cross between a smartphone
and a software defined radio with an open hardware and software license.

.. image:: https://raw.github.com/testaco/whitebox/master/docs/_images/bravotop-mid.jpg
    :alt: Whitebox Bravo
    :align: center

Directory Structure
-------------------

* board: Schematics and diagrams for the Whitebox board
* cmake: Toolchains and modules for cmake
* docs: Documentation for the project
* driver: The Kernel driver for the Whitebox radio board
* gnuradio: Tools for using the Whitebox as a GNURadio peripheral
* hdl: Hardware description in MyHDL for the FPGAs DSP flow
* lib: A userspace library for interacting with the radio board
* linux-cortexm: Tools for building the uClinux Kernel for the ARM Cortex-M
  class of processors.
* util: Utility Python scripts

Getting Started
---------------
::

    # Get the repo
    $ git clone https://github.com/testaco/whitebox
    $ cd whitebox
    # Bootstrap your toolchain
    whitebox$ sh bootstrap.sh
    whitebox$ cd build
    # Configure with cmake, don't forget to cross compile
    whitebox/build$ cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchains/arm_cortex_m3_native.cmake ..
    # Build the user space library
    whitebox/build$ make
    # Patch the kernel, do this only ONCE
    whitebox/build$ make linux_patch
    # Build the kernel
    whitebox/build$ make linux
    # Build the drivers
    whitebox/build$ make drivers
    # Generate and test the hdl Verilog code
    whitebox/build$ make hdl
    # Build the documentation
    whitebox/build$ make docs


Contact
-------

To keep up-to-date, check out the Facebook page: http://facebook.com/whiteboxradio

Or follow the designer on Twitter: http://twitter.com/testa
