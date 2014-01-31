Installing
==========

Downloading a compiled image is not ready yet.  For now, please follow the `Building` instructions before reaching this point.

NFS
---

Windows
Mac
Linux

TFTP
----

Windows
Mac
Linux

Custom Kernel Boot
------------------

This step depends on you properly setting up TFTP on your machine.

To begin, plug the USB serial into your computer and open up a serial console:

$ screen /dev/ttyUSB0 115200

You should see the Linux kernel boot, and then a shell prompt.  If it doesn't
show up try pressing Enter.  Issue a reset command.

~# reboot

On reboot, make sure to hit a key when it asks to stop autoboot.  You will
fall into the U-Boot prompt.

Execute the following commands from U-Boot (your constants will vary.)

A2F500-SOM> setenv ipaddr 192.168.220.3
A2F500-SOM> setenv serverip 192.168.220.1
A2F500-SOM> setenv image whitebox.uImage
A2F500-SOM> saveenv

Then, either boot onetime from the network:

A2F500-SOM> run netboot

Or, load this kernel image into flash:

A2F500-SOM> run update
A2F500-SOM> reset

Flashing the FPGA
-----------------

Flashing the FPGA is incredibly easy once you have NFS set up.

.. warning::
    There's a chance that this process could brick your device; and that you
    will need a FlashPro JTAG programmer to recover.  This has
    happened to me once in hundreds of flashings, so it is rare, but possible.

To flash, issue the following command from the USB serial console:

~# iap_tool /mnt/whitebox/build/hdl/TOPLEVEL.dat

And you should see it then succesfully flash the FPGA in about one minute.

I've found that you need to pull the power from the device before the gate
array is guaranteed to be in a new stable state.  Pushing the reset button
will not do that, and I've wasted hours trying to debug HDL issues that
only needed a simple reboot to go away.

If it doesn't exit with a status code 0, or hangs forever, then the best
thing to do is try and cycle the power by unplugging the device and then plugging
it back in.  Every time that I've had this happen, when I restarted
and attempted the flashing again, everything worked fine.  Except for once.

One time, I was not so lucky.  On reboot, all I saw was garbage come out on
the serial console.  If this happens, then you need to get a FlashPro cable
and flash an original U-Boot image over JTAG.
