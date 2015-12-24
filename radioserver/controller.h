#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

enum controller_state {
    standby,
    idle,
    receive,
    transmit,
};


void controller_init();
int controller_timeout();
void controller_task();
void controller_task(controller_state new_state);


const bool controller_get_led();
void controller_set_led(const bool new_led);

const int controller_get_bpf();
void controller_set_bpf(const int new_bpf);

const bool controller_get_lna();
void controller_set_lna(const bool new_lna);

const bool controller_get_noise();
void controller_set_noise(const bool new_noise);

const bool controller_get_mute_lo();
void controller_set_mute_lo(const bool new_mute_lo); // TODO remove!

const float controller_get_frequency();
void controller_set_frequency(const float frequency);

const char* controller_get_if_bw();
void controller_set_if_bw(const char* if_bw);

#endif
