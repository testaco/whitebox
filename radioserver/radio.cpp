#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <stdint.h>
#include <iostream>
#include <math.h>
#if OPENSLL_FOUND
#include <openssl/err.h>
#endif
#include <poll.h>
#include <sys/time.h>
#include "cJSON.h"
#include "radio.h"
#include "repeater.h"
#include "radio_context.h"
#include "controller.h"

radio_context *
radio_start(client_context * client, const client_info * info)
{
  std::cerr << "Radio start" << std::endl;
  radio_context * radio = new radio_context(client, info);
  repeater::get_instance().connect(radio);
  return radio;
} 

void
radio_end(radio_context * radio, const client_info *)
{
  std::cerr << "Radio end" << std::endl;
  repeater::get_instance().disconnect(radio);
  delete radio;
}

void
radio_get_status(radio_context *, const client_info *, cJSON * json)
{
    std::cerr << "Radio get status" << std::endl;
    cJSON_AddBoolToObject(json, "led", controller_get_led());
    cJSON_AddNumberToObject(json, "bpf", controller_get_bpf());
    cJSON_AddBoolToObject(json, "lna", controller_get_lna());
    cJSON_AddBoolToObject(json, "noise", controller_get_noise());
    cJSON_AddBoolToObject(json, "mute_lo", controller_get_mute_lo());
    cJSON_AddNumberToObject(json, "frequency", controller_get_frequency());
    cJSON_AddStringToObject(json, "if_bw", controller_get_if_bw());
#if 0
    cJSON_AddStringToObject(json, "mode", modem_get_mode());
    cJSON_AddNumberToObject(json, "vga", modem_get_vga());
    cJSON_AddNumberToObject(json, "rssi", modem_get_rssi());
    cJSON_AddNumberToObject(json, "temp", modem_get_temp());
    cJSON_AddNumberToObject(json, "voltage", modem_get_voltage());
    cJSON_AddNumberToObject(json, "locked", modem_get_locked_status());
    cJSON_AddBoolToObject(json, "pa", modem_get_pa());
#endif
}

void
radio_set(radio_context *, const client_info *, const cJSON * json)
{
  std::cerr << "Radio set status" << std::endl;
    std::cerr << cJSON_Print((cJSON*)json);
    const cJSON * led_obj = cJSON_GetObjectItem((cJSON*)json, "led");
    if (led_obj) {
        bool led = led_obj->valueint;
        controller_set_led(led);
    }
    const cJSON * bpf_obj = cJSON_GetObjectItem((cJSON*)json, "bpf");
    if (bpf_obj) {
        int bpf = bpf_obj->valueint;
        controller_set_bpf(bpf);
    }
    const cJSON * lna_obj = cJSON_GetObjectItem((cJSON*)json, "lna");
    if (lna_obj) {
        bool lna = lna_obj->valueint;
        controller_set_lna(lna);
    }
    const cJSON * noise_obj = cJSON_GetObjectItem((cJSON*)json, "noise");
    if (noise_obj) {
        bool noise = noise_obj->valueint;
        controller_set_noise(noise);
    }
    const cJSON * mute_lo_obj = cJSON_GetObjectItem((cJSON*)json, "mute_lo");
    if (mute_lo_obj) {
        bool mute_lo = mute_lo_obj->valueint;
        controller_set_mute_lo(mute_lo);
    }
    const cJSON * freq_obj = cJSON_GetObjectItem((cJSON*)json, "frequency");
    if (freq_obj) {
        float frequency = freq_obj->valuedouble;
        controller_set_frequency(frequency);
    }
    const cJSON * if_bw_obj = cJSON_GetObjectItem((cJSON*)json, "if_bw");
    if (if_bw_obj) {
        const char* if_bw = if_bw_obj->valuestring;
        controller_set_if_bw(if_bw);
    }
#if 0
    const cJSON * mode_obj = cJSON_GetObjectItem((cJSON*)json, "mode");
    if (mode_obj) {
        char* mode = mode_obj->valuestring;
        modem_set_mode(mode);
    }
    const cJSON * vga_obj = cJSON_GetObjectItem((cJSON*)json, "vga");
    if (vga_obj) {
        int vga = vga_obj->valueint;
        modem_set_vga(vga);
    }
    const cJSON * pa_obj = cJSON_GetObjectItem((cJSON*)json, "pa");
    if (pa_obj) {
        bool pa = pa_obj->valueint;
        modem_set_pa(pa);
    }
 #endif
}

void
radio_receive(radio_context * radio, const client_info *)
{
    std::cerr << "Radio receive" << std::endl;
    repeater::get_instance().start_receive();
}

void
radio_transmit(radio_context * radio, const client_info *)
{
    std::cerr << "Radio transmit" << std::endl;
    repeater::get_instance().start_transmit(radio);
}

void
radio_data_in(
 radio_context *	/* radio */,
 const client_info *	/* info */,
 unsigned int		type,
 const void *		data,
 size_t		length)
{
#if 0
    const int16_t *audio_data = (int16_t *)data;

    //std::cerr << std::endl;
    //fprintf(stderr, "\n");
#endif
    //modem::get_instance().transmit(data, length);
    //radio->repeater->transmit_data(type, data, length);
    //std::cerr << "TRANSMIT DATA type=" << type << " length=" << length << std::endl;
    // TODO: do this as a block of data
    //for (int i = 0; i < length / 4; ++i) {
    //    std::cerr << ' ' << ((uint32_t*)data)[i];
    //}
}

