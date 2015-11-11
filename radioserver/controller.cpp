#include <stdint.h>
#include <iostream>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

// TODO: linux only
#include <sys/timerfd.h>
#include <poll.h>

#include "dsp.h" // TODO: move this into the modem
#include "radio.h"
#include "controller.h"
#include "repeater.h"

static timespec millis_ts;

int millis() {
    timespec new_ts;
    clock_gettime(CLOCK_MONOTONIC, &new_ts);
    int millis = (new_ts.tv_sec - millis_ts.tv_sec) * 1000L +
        (new_ts.tv_nsec - millis_ts.tv_nsec) / 1000000L;
    millis_ts = new_ts;
    return millis;
}

void controller_init() {
    millis();
}

static const char * const controller_state_strings[] = {
    "Standby",
    "Idle",
    "Receive",
    "Receive Callback",
    "Transmit",
};

static int controller_cur_timeout;
static controller_state controller_cur_state;

static int controller_receive_fd = -1;

int controller_timeout() {
    return controller_cur_timeout;
}

static void receive_cb(pollfd * pollfd, void * ) {
    static uint32_t fcw = freq_to_fcw(1440, 8192/2);
    static uint32_t phase = 0;
    uint64_t count;
    read(pollfd->fd, &count, sizeof(uint64_t));
    // How many times the timer has happened; each one is 125ms, so there's 8 per second.  At 8192sps, that
    size_t samples_count = count * (8192 / 8);
    size_t samples_byte_length = samples_count * sizeof(uint32_t);
    uint32_t * data = new uint32_t[samples_count];
    for (int i = 0; i < samples_count; ++i) {
        data[i] = sincos16c(fcw, &phase);
        //awgn32(&data[i]);
    }
    repeater::get_instance().receive_cb(1, data, samples_byte_length + 4);
}

void timerfd_periodic(int fd, int ms) {
    struct itimerspec spec;
    spec.it_interval.tv_sec = 0;
    std::cerr << ms << " ";
    while (ms > 1000) {
        spec.it_interval.tv_sec += 1;
        ms -= 1000;
    }
    spec.it_interval.tv_nsec = ms * 1000000;
    spec.it_value.tv_sec = spec.it_interval.tv_sec;
    spec.it_value.tv_nsec = spec.it_interval.tv_nsec;
    std::cerr << spec.it_interval.tv_sec << " " << spec.it_interval.tv_nsec << std::endl;
    if(timerfd_settime(controller_receive_fd, 0, &spec, NULL) < 0) {
        // TODO: error
        return;
    }
    poll_start_fd(fd, POLLIN, receive_cb, NULL);
}

void timerfd_end(int fd) {
    poll_end_fd(fd);
    close(fd);
}

void controller_task() {
    controller_state next_state;
    int next_timeout;

    int ms = millis();
    std::cerr << "--------------" << std::endl;
    std::cerr << "---- Run Tasks elapsed=" << ms << " state="
              << controller_state_strings[controller_cur_state] << std::endl;

    switch(controller_cur_state) {
        case receive:
            assert(controller_receive_fd == -1);
            controller_receive_fd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
            assert(controller_receive_fd >= 0);
            timerfd_periodic(controller_receive_fd, 125);
            next_state = idle;
            next_timeout = -1;
            break;
        case transmit:
            next_state = transmit;
            next_timeout = 125;
            break;
        case standby:
            if (controller_receive_fd >= 0) {
                timerfd_end(controller_receive_fd);
                controller_receive_fd = -1;
            }
        case idle:
        default:
            next_state = idle;
            next_timeout = -1;
            break;
    };

    controller_cur_state = next_state;
    controller_cur_timeout = next_timeout;
    std::cerr << "---- End Tasks timeout="
              << controller_cur_timeout << " state="
              << controller_state_strings[controller_cur_state] << std::endl;
    std::cerr << "--------------" << std::endl;
    std::cerr << std::endl;

}

void controller_task(controller_state new_state) {
    controller_cur_state = new_state;
    controller_task();
}

