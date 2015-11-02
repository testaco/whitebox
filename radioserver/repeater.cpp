#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <iostream>

#include "radio.h"
#include "radio_context.h"
#include "repeater.h"
#include "controller.h"

void repeater::connect(radio_context * context) {
    if (clients.size() == 0) {
        controller_task(idle);
    }

    clients.push_back(context);
    std::cerr << "New client";
    std::cerr << " (" << clients.size() << " clients)" << std::endl;
}

void repeater::disconnect(radio_context * context) {
    clients.remove(context);
    std::cerr << "Close client";
    std::cerr << " (" << clients.size() << " clients)" << std::endl;

    if (clients.size() == 0) {
        controller_task(idle);
    }
}

void repeater::start_receive() {
    controller_task(receive);
}

void repeater::receive_cb(unsigned int type, const void * data, size_t length) {
    std::list<radio_context *>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        // TODO ewww.
        WriteBuffer * buffer = new WriteBuffer(length, 1);
        memcpy(buffer->data(), data, length);
        server_data_out((*it)->get_client(), buffer);
    }
}

void repeater::start_transmit(radio_context * radio) {
    // TODO: if user has permissions
    // Try to get the repeater lock
    if (try_transmit_lock(radio))
        controller_task(transmit);
}

bool repeater::try_transmit_lock(radio_context * radio) {
    if (transmitter == NULL) {
        transmitter = radio;
        return true;
    }
    return false;
}
