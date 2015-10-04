#ifndef __MODEM_H__
#define __MODEM_H__

#include "resources.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct resource_ops modem_ops;

void modem_transmit();
void modem_receive();
void modem_standby();

void modem_service_fd(short revents);
void modem_write();
void modem_read();
void modem_recover();

float modem_get_frequency();
void modem_set_frequency(float frequency);

const char* modem_get_mode();
void modem_set_mode(const char* new_mode);

const bool modem_get_lna();
void modem_set_lna(const bool lna);

const int modem_get_vga();
void modem_set_vga(const int vga);

const char* modem_get_if_bw();
void modem_set_if_bw(const char* if_bw);

const int modem_get_bpf();
void modem_set_bpf(const int bpf);

const float modem_get_rssi();
const float modem_get_temp();
const float modem_get_voltage();
const bool modem_get_locked_status();

const bool modem_get_pa();
void modem_set_pa(const bool pa);

const bool modem_get_led();
void modem_set_led(const bool led);

// modulator_write
// demodulator_read

#ifdef __cplusplus
}
#endif

#endif /* __MODEM_H__ */
