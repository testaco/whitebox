set -e
echo "Whitebox Project Installer"
echo "Version 0.2 BRAVO"
echo "Copyright (C) 2013 Chris Testa KD2BMH"
echo "Released under the GPL"
echo
echo "This bootstrap script will help you get your local machine ready for"
echo "development.  It should only take a few minutes to get set up."
echo

echo "But first, can I entice you to supply your email to keep up to date on"
echo "the Whitebox project?  There's lots of exciting developments to come"
echo "and by keeping in touch I can learn how this project can better serve you."
echo

echo "Bootstrapping, please be patient"

LINUX_CORTEXM_URL="http://radio.testa.co/bin/linux-A2F-1.11.0.tar.bz2"
TOOLCHAIN_URL="http://www.codesourcery.com/sgpp/lite/arm/portal/package6503/public/arm-uclinuxeabi/arm-2010q1-189-arm-uclinuxeabi-i686-pc-linux-gnu.tar.bz2"

rm -rf build

mkdir build
cd build
echo "Installing the linux cortexm support package"
wget -qO- $LINUX_CORTEXM_URL | tar jxv

cd linux-cortexm-1.11.0/tools

echo "Installing the toolchain"
wget -qO- $TOOLCHAIN_URL | tar jxv

cd ../..

echo
echo "Bootstrap complete"
echo
echo "Add the ARM toolchain to your PATH, then configure with cmake, and finally build with make:"
echo "    $ echo 'PATH=\"`pwd`/linux-cortexm-1.11.0/tools/bin:`pwd`/linux-cortexm-1.11.0/tools/arm-2010q1/bin:\$PATH\"' >> ~/.profile"
echo "    $ cd build && cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchains/arm_cortex_m3_native.cmake .."
echo "    $ make linux"
echo
echo "This will give you a linux kernel image at build/whitebox.uImage which can be loaded via TFTP following these steps."
echo "    http://radio.testa.co/installing.py#tftp"
