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

#email_prompt() {
#    read -p "Your email address: " email
#    echo "Thanks for signing up!"
#}
#
#while true; do
#    read -p "Join the whitebox-announce mailing list? [yn] " choice
#    case $choice in 
#        y|Y ) email_prompt; break;;
#        n|N ) echo "No worries, enjoy exploring!"; break;;
#        * ) echo "Please select y or n";;
#    esac
#done

echo "Bootstrapping, please be patient"

LINUX_CORTEXM_FILE="/home/testa/Downloads/linux-cortexm-A2F-1.9.0.tar.bz2"
TOOLCHAIN_URL="http://www.codesourcery.com/sgpp/lite/arm/portal/package6503/public/arm-uclinuxeabi/arm-2010q1-189-arm-uclinuxeabi-i686-pc-linux-gnu.tar.bz2"

rm -rf build

mkdir build
cd build
echo "Installing the linux support package"
tar jxfv $LINUX_CORTEXM_FILE

cd linux-cortexm-1.9.0/tools

echo "Installing the toolchain"
wget -qO- $TOOLCHAIN_URL | tar jxv

cd ../..

echo

echo "Add the ARM toolchain to your PATH, then configure with cmake:"
echo "    $ echo 'PATH=\"`pwd`/linux-cortexm-1.9.0/tools/bin:`pwd`/linux-cortexm-1.9.0/tools/arm-2010q1/bin:\$PATH\"' >> ~/.profile"
echo "    $ cd build && cmake .."

echo

echo "Bootstrap complete"

