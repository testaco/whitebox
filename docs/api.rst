Application Programming Interface
=================================

The development system broken up into three distinct parts.

Register Transfer Logic
-----------------------

This code is written completely in Python, and then transverted into Verilog.  An entire simulation system has been built to verify the correctness of the generated Verilog.  With numpy, scipy, matplotlib, and sympy, Python is capable of being an end-to-end tool for building complex embedded systems.

.. toctree::
    
    dsp
    whitebox_hdl
    
Linux Device Driver
-------------------

The device driver is the Kernel level code that interfaces between a user's application and the RTL.  It uses standard Linux interfaces to expose an easy to use character device, and plenty of knobs to control the system in real time.

.. toctree::

    driver

C User Space API
----------------

Ultimately, userspace programs drive the system.  These can be written in C or C++ and compiled for the ARM Cortex-M3 core.  These API's provide a simple means of manipulating the device driver, and ultimately the entire radio signal chain.

.. toctree::

    cmx991
    adf4351
