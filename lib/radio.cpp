#include <iostream>
#include <cstdlib>
#include <string.h>
#include <cstdio>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include "cJSON.h"
#include "radio.h"
#include "modem.h"
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
 unsigned char * 	/* type */,
 const void *		data,
 int			length)
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
radio_end(radio_context * radio)
{
  modem_standby();
  std::cerr << "Close client." << std::endl;
  delete radio;
}

void
radio_get_status(radio_context *, cJSON * json)
{
    cJSON_AddNumberToObject(json, "frequency", modem_get_frequency());
    cJSON_AddStringToObject(json, "mode", modem_get_mode());
}

void
radio_receive(radio_context *)
{
    modem_receive();
}

void
radio_set(radio_context *, const cJSON * json)
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
}

radio_context *
radio_start(client_context * client)
{
  std::cerr << "New client." << std::endl;
  return new radio_context(client);
}

void
radio_transmit(radio_context *)
{
    modem_transmit();
}

radio_context::~radio_context()
{
  //modem_standby();
}
