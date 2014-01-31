Test Circuit
============

Testing the system is accomplished via the test_* commands.

The first test to run is test_driver.  This will make sure that the driver is loaded correctly and that the character device interface conforms to POSIX standards.

The next tests make sure that the library routines to manipulate the register files work correctly.  They're test_adf4351 and test_cmx991.

The next test verifys that the Whitebox HDL and RF card are functioning correctly together.  It does this by first locking all of the PLLs at all of the Amateur bands between 100MHz and 1000MHz.  Next, it streams reads and writes from the character device to the HDL.  A loopback is used to test both transmit and receive DSP chain interfaces between the driver and the HDL, including DMA.

Transmitter Calibration
-----------------------

Since the device uses an Analog Quadrature Modulator, a number of real world parasitics can be compensated for by a calibration routine.  The transmitter is calibrated by connecting the TX port of the daughter card to a specturm analyzer, or the receiver of another radio.

First, a static DC value of 0 is sent out on the DAC, and you will observe carrier leakage.  You can adjust the values of the I and Q DC offset by using the arrow keys.  When you've minimized the carrier leakage, press Enter.

Next, a lower sideband singal will be sent through the radio.  Again, manipulate the DC offset to minimize the carrier.  Press Enter.

Next, the lower sideband signal will still be applied, but the goal is to minimize the upper (undesired) sideband power.  We will first adjust the phase offset between the I and Q signal components.

Finally, we will take one more stab at reducing undesired sideband power.  To do this, we will adjust the gain between the I and Q components using the arrow keys.  Adjust so that way the undesired sideband power is at a minimum.  Press enter.

You will again be able to adjust the phase, to see if you can minimize it further.  When you are finally done, press q or Esc to quit.

