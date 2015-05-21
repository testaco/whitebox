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

// modulator_write
// demodulator_read

#ifdef __cplusplus
}
#endif

#endif /* __MODEM_H__ */
