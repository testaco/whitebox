#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

enum controller_state {
    idle,
    receive,
    receive_callback,
    transmit,
};


void controller_init();
int controller_timeout();
void controller_task();
void controller_task(controller_state new_state);

#endif
