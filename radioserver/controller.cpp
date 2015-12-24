#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

// TODO: linux only
#include <sys/timerfd.h>
#include <poll.h>

#include "whitebox.h"
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

static whitebox *wb = NULL;
static bool txing = false;
static bool rxing = false;

void controller_init() {
    millis();
    std::cerr << "Opening the whitebox" << std::endl;
    assert(wb == NULL);
    wb = (whitebox*)malloc(sizeof(*wb));
    whitebox_init(wb);
    if (whitebox_open(wb, "/dev/whitebox", O_RDWR | O_NONBLOCK, 10000) < 0) {
        std::cerr << "Error: couldn't open the whitebox" << std::endl;
        free(wb);
        wb = NULL;
        return;
    }
    if (whitebox_mmap(wb) < 0) {
        std::cerr << "Error: couldn't mmap the whitebox" << std::endl;
        free(wb);
        wb = NULL;
        return;
    }
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
    size_t samples_count = 1; // TODO count * (8192 / 8);
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
    //std::cerr << "--------------" << std::endl;
    //std::cerr << "---- Run Tasks elapsed=" << ms << " state="
    //          << controller_state_strings[controller_cur_state] << std::endl;

    switch(controller_cur_state) {
        case receive:
            if (whitebox_rx(wb, 450e6) < 0) // TODO
                std::cerr << "Receive start failed!" << std::endl;
            std::cerr << "Setting up the whitebox fd for receive" << std::endl;
            poll_start_fd(whitebox_fd(wb), POLLIN | POLLERR, receive_cb, NULL);
            rxing = true;
            txing = false;
            next_state = idle;
            next_timeout = -1;
            break;
            // TODO
            assert(controller_receive_fd == -1);
            controller_receive_fd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
            assert(controller_receive_fd >= 0);
            timerfd_periodic(controller_receive_fd, 125);
            break;
        case transmit:
            if (whitebox_tx(wb, 450e6) < 0) // TODO
                std::cerr << "Transmit start failed!" << std::endl;
            std::cerr << "Setting up the whitebox fd for transmit" << std::endl;
            txing = true;
            rxing = false;
            next_state = idle;
            next_timeout = -1;
            break;
        case standby:
            txing = rxing = false;
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
    //std::cerr << "---- End Tasks timeout="
    //          << controller_cur_timeout << " state="
    //          << controller_state_strings[controller_cur_state] << std::endl;
    //std::cerr << "--------------" << std::endl;
    //std::cerr << std::endl;

}

void controller_task(controller_state new_state) {
    controller_cur_state = new_state;
    controller_task();
}

const bool controller_get_led() {
    return wb->led;
}

void controller_set_led(const bool new_led) {
    if (wb->led != new_led) {
        wb->led = new_led;
        std::cerr << "LED" << new_led << std::endl;
        if (wb->led)
            whitebox_led_on();
        else
            whitebox_led_off();
    }
}

const int controller_get_bpf() {
    return wb->bpf;
}

void controller_set_bpf(const int bpf) {
    if (bpf != wb->bpf) {
        wb->bpf = bpf;
        whitebox_gateway_set(wb);
    }
}

const bool controller_get_lna() {
    return wb->lna;
}

void controller_set_lna(const bool new_lna) {
    if (wb->lna != new_lna) {
        wb->lna = new_lna;
        // if (rxing) { TODO only do this if we're actually transmitting
            std::cerr  << "LNA" << new_lna << std::endl;
            whitebox_rx_config(wb);
        //}
    }
}

const bool controller_get_noise() {
    return wb->noise;
}

void controller_set_noise(const bool new_noise) {
    if(wb->noise != new_noise) {
        wb->noise = new_noise;
        std::cerr << "NOISE " << new_noise << std::endl;
        whitebox_rx_config(wb);
    }
}

static bool mute_lo = false;
const bool controller_get_mute_lo() {
    return mute_lo;
}

void controller_set_mute_lo(const bool new_mute_lo) {
    if(mute_lo != new_mute_lo) {
        mute_lo = new_mute_lo;
        std::cerr << "MUTE LO " << mute_lo << std::endl;
        if (mute_lo)
            whitebox_mute_lo(wb);
    }
}

const float controller_get_frequency() {
    return wb->frequency;
}

void controller_set_frequency(const float frequency) {
    std::cerr << "setting frequency " << wb->frequency << " " << frequency << std::endl;
    if (wb->frequency != frequency) {
        std::cerr << "new frequency " << frequency << std::endl;
        wb->frequency = frequency;
        if (txing) whitebox_tx_fine_tune(wb, frequency);
        if (rxing) whitebox_rx_fine_tune(wb, frequency);
    }
}

const char* _string_for_if_bw(if_filter_bw_t if_bw) {
    switch (wb->if_bw) {
        case KHZ_30: return "30KHz";
        case KHZ_100: return "100KHz";
        default: return "1MHz";
    }
}

const if_filter_bw _enum_for_if_bw(const char* if_bw) {
    if (strcmp(if_bw, "30KHz") == 0) return KHZ_30;
    if (strcmp(if_bw, "100KHz") == 0) return KHZ_100;
    return MHZ_1;
}

const char* controller_get_if_bw() {
    return _string_for_if_bw(wb->if_bw);
}

void controller_set_if_bw(const char* if_bw) {
    if (strcmp(_string_for_if_bw(wb->if_bw), if_bw) != 0) {
        wb->if_bw = _enum_for_if_bw(if_bw);
        if (rxing) {
            std::cerr  << "if_bw" << if_bw << std::endl;
            whitebox_rx_config(wb);
        }
    }
}

