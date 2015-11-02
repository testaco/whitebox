#include <stdint.h>
#include <iostream>

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
    "Idle",
    "Receive",
    "Receive Callback",
    "Transmit",
};

static int controller_cur_timeout;
static controller_state controller_cur_state;

int controller_timeout() {
    return controller_cur_timeout;
}

static void receive_cb(const size_t length) {
    uint32_t * data = new uint32_t[length];
    for (int i = 0; i < length; ++i) {
        data[i] = 1;
    }
    repeater::get_instance().receive_cb(1, data, length * sizeof(*data));
}

void controller_task() {
    controller_state next_state;
    int next_timeout;

    int ms = millis();
    std::cerr << "--------------" << std::endl;
    std::cerr << "---- Run Tasks elapsed=" << ms << " state="
              << controller_state_strings[controller_cur_state] << std::endl;

    size_t net_samples_length = 8192 * ms * 1000;

    switch(controller_cur_state) {
        case receive:
            next_state = receive_callback;
            next_timeout = 0;
            break;
        case receive_callback:
            receive_cb(net_samples_length);
            next_state = receive_callback;
            next_timeout = 125;
            break;
        case transmit:
            next_state = transmit;
            next_timeout = 125;
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

