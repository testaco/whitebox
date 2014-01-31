Platform
--------

The platform exposes the Digital Up Converter and Digital Down Converter streams with an Object Oriented, Kernel friendly interface.

.. c:type:: struct whitebox_device
    
    Main bookkeeping for the device, memory addresses, pin locations, the flow graph, and statistics.

.. c:type:: struct whitebox_platform_data
    
    Contains all of the constants for pins and DMA channel allocations.

.. c:type:: struct whitebox_stats
    
    Statistics can be tracked and exposed on the debugfs interface in the directory /sys/kernel/debug/whitebox/.

Flow Graph
----------

The Digital Singal Processing Flow Graph idiom is used throughout the driver.  On write operations, the userspace application writes to the User Source block, which gets the data from userspace and notifies the DSP chain that data is available.  The corresponding sink block is called the RF Sink, and it uses DMA to copy the data from kernelspace to the Whitebox Peripheral.

Similarly, for read, the userspace application is the final sink in the DSP chain from the Whitebox Peripheral and through the RF Source.  Again, DMA is used to get data from the peripheral to the processor.

Character Device
----------------

The character device is registered at /dev/whitebox.  It exposes the following Unix file commands:

.. c:function:: open

.. c:function:: close

.. c:function:: read

.. c:function:: write

.. c:function:: fsync

.. c:function:: ioctl

