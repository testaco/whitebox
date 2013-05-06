from myhdl import instance, delay

def drive_system_clock(system_clock, period):
    low_time = int(period / 2)
    high_time = period - low_time

    @instance
    def drive_system_clock():
        while True:
            yield delay(low_time)
            system_clock.next = 1
            yield delay(high_time)
            system_clock.next = 0

    return drive_system_clock

def drive_reset(reset, active_low=True):
    @instance
    def drive_reset():
        reset.next = 0 if active_low else 1
        yield delay(10)
        reset.next = 1 if active_low else 0

    return drive_reset
