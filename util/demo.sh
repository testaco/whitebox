insmod /mnt/whitebox/build/driver/pdma.ko
insmod /mnt/whitebox/build/driver/whitebox.ko
while [ 1 ]
do
    /mnt/whitebox/build/lib/rfcat -v -f 144.39e6 -r 50000 --dds /mnt/whitebox/hdl/cq_rf.samples
    /mnt/whitebox/build/lib/rfcat -v -f 144.39e6 -r 50000 /mnt/whitebox/hdl/sin.samples
    /mnt/whitebox/build/lib/rfcat -v -f 144.39e6 -r 50000 --filter /mnt/whitebox/hdl/sin_3db.samples
    /mnt/whitebox/build/lib/rfcat -v -f 144.39e6 -r 50000 /mnt/whitebox/hdl/cq_rf.samples
    /mnt/whitebox/build/lib/rfcat -v -f 144.39e6 -r 50000 --filter /mnt/whitebox/hdl/cq_rf.samples
done
