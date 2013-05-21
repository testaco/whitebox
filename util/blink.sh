# Very simple demo that toggles one of the LED's on the board.
echo 3 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio3/direction
while date; do
    echo 1 > /sys/class/gpio/gpio3/value
    sleep 1
    echo 0 > /sys/class/gpio/gpio3/value
    sleep 1
; done
