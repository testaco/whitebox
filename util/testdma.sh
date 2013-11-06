insmod /mnt/whitebox/build/driver/pdma.ko
insmod /mnt/whitebox/build/driver/whitebox.ko
/mnt/whitebox/build/lib/rfcat -v -f 440.39e6 -r 1000000 --loop /mnt/whitebox/hdl/sin.samples
