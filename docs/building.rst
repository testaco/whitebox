Building
========

Setting up a Linux VM (optional)
--------------------------------

Cloning the source code
-----------------------

~$ git clone https://github.com/testaco/whitebox.git

Bootstrapping
-------------

~/whitebox$ sh bootstrap.sh

This will take a bit of time, as it is going to install the SmartFusion gcc toolchain, as well as the Emcraft uClinux distribution.

Compiling the Kernel
--------------------

After the bootstrap, add the ARM toolchain to your PATH by running the final commands or adding the appopriate statement to your .profile.

~/whitebox/build$ cd build && cmake ..
~/whitebox/build$ make

This will compile the kernel and leave the bootable image as whitebox.uImage.

You can then load this image by setting up NFS and doing a Custom Kernel Booat as described in :doc:`/installing`.

Building the FPGA Image
-----------------------

First step is to build the hdl files:

~/whitebox/build$ make hdl

Next, copy the manifest of Verilog files into the Libero project.  These files include whitebox_toplevel.v and whitebox.v and should be put in the folder libero/hdl.

Run the Libero compilation procedure.  It will take 20-40 minutes to complete.

When it's done, open up FlashPro on the project, and go to File -> Export -> Single Programming File.  This will create the TOPLEVEL.dat file which you can then install by following the Flashing the FPGA guide in :doc:`/installing`.

