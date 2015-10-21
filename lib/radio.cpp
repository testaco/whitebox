#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>
#include <math.h>
#if OPENSLL_FOUND
#include <openssl/err.h>
#endif
#include <poll.h>
#include <sys/time.h>
#include "cJSON.h"
#include "modem.h"
#include "radio.h"
#include "resources.h"

// Add members to radio_context as you wish.
class radio_context {
private:
  client_context * const client;

  radio_context(const radio_context &);
  radio_context &	operator =(const radio_context &);

public:
    radio_context(client_context * c) : client(c) {
    };
    ~radio_context();
};

void
radio_data_in(
 radio_context *	/* radio */,
 const client_info *	/* info */,
 unsigned int		/* type */,
 const void *		data,
 size_t		length)
{
    const int16_t *audio_data = (int16_t *)data;

    // TODO: do this as a block of data
    for (int i = 0; i < length / 2; ++i) {
        //std::cerr << ' ' << audio_data[i];
        //fprintf(stderr, " %d", audio_data[i]);

        sink(audio_data[i]);
    }
    //std::cerr << std::endl;
    //fprintf(stderr, "\n");
}

void
radio_end(radio_context * radio, const client_info *)
{
  modem_standby();
  std::cerr << "Close client." << std::endl;
  delete radio;
}

void
radio_get_status(radio_context *, const client_info *, cJSON * json)
{
    cJSON_AddNumberToObject(json, "frequency", modem_get_frequency());
    cJSON_AddStringToObject(json, "mode", modem_get_mode());
    cJSON_AddBoolToObject(json, "lna", modem_get_lna());
    cJSON_AddNumberToObject(json, "vga", modem_get_vga());
    cJSON_AddStringToObject(json, "if_bw", modem_get_if_bw());
    cJSON_AddNumberToObject(json, "bpf", modem_get_bpf());
    cJSON_AddNumberToObject(json, "rssi", modem_get_rssi());
    cJSON_AddNumberToObject(json, "temp", modem_get_temp());
    cJSON_AddNumberToObject(json, "voltage", modem_get_voltage());
    cJSON_AddNumberToObject(json, "locked", modem_get_locked_status());
    cJSON_AddBoolToObject(json, "pa", modem_get_pa());
    cJSON_AddBoolToObject(json, "led", modem_get_led());
}

void
radio_receive(radio_context *, const client_info *)
{
    modem_receive();
}

void
radio_set(radio_context *, const client_info *, const cJSON * json)
{
    const cJSON * freq_obj = cJSON_GetObjectItem((cJSON*)json, "frequency");
    if (freq_obj) {
        float frequency = freq_obj->valuedouble;
        modem_set_frequency(frequency);
    }
    const cJSON * mode_obj = cJSON_GetObjectItem((cJSON*)json, "mode");
    if (mode_obj) {
        char* mode = mode_obj->valuestring;
        modem_set_mode(mode);
    }
    const cJSON * lna_obj = cJSON_GetObjectItem((cJSON*)json, "lna");
    if (lna_obj) {
        bool lna = lna_obj->valueint;
        modem_set_lna(lna);
    }
    const cJSON * vga_obj = cJSON_GetObjectItem((cJSON*)json, "vga");
    if (vga_obj) {
        int vga = vga_obj->valueint;
        modem_set_vga(vga);
    }
    const cJSON * if_bw_obj = cJSON_GetObjectItem((cJSON*)json, "if_bw");
    if (if_bw_obj) {
        const char* if_bw = if_bw_obj->valuestring;
        modem_set_if_bw(if_bw);
    }
    const cJSON * bpf_obj = cJSON_GetObjectItem((cJSON*)json, "bpf");
    if (bpf_obj) {
        int bpf = bpf_obj->valueint;
        modem_set_bpf(bpf);
    }
    const cJSON * pa_obj = cJSON_GetObjectItem((cJSON*)json, "pa");
    if (pa_obj) {
        bool pa = pa_obj->valueint;
        modem_set_pa(pa);
    }
    const cJSON * led_obj = cJSON_GetObjectItem((cJSON*)json, "led");
    if (led_obj) {
        bool led = led_obj->valueint;
        modem_set_led(led);
    }
}

radio_context *
radio_start(client_context * client, const client_info *)
{
  std::cerr << "New client." << std::endl;
  return new radio_context(client);
} 
void
radio_transmit(radio_context *, const client_info *)
{
    modem_transmit();
}

radio_context::~radio_context()
{
  //modem_standby();
}
