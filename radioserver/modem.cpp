#include <cstdlib>
#include <iostream>
#include <stdint.h>
#include <poll.h>
#include <list>

#include "radio.h"
#include "radio_context.h"
#include "modem.h"

void modem::connect(radio_context * context) {
    connections.push_back(context);
    std::cerr << "New client";
    std::cerr << " (" << connections.size() << " connections)" << std::endl;
}

void modem::disconnect(radio_context * context) {
    connections.remove(context);
    std::cerr << "Close client";
    std::cerr << " (" << connections.size() << " connections)" << std::endl;

    if (connections.size() == 0) {
        standby();
    }
}

void modem::standby() {
    std::cerr << "Standby" << std::endl;
}

void modem::start_receive() {
    std::cerr << "Radio command to receive" << std::endl;
    // TODO: schedule a timer here to call modem_receive_callback
}

void modem::modem_receive_callback() {
    std::list<radio_context *>::iterator it;
    for (it = connections.begin(); it != connections.end(); ++it) {
        WriteBuffer * buffer = new WriteBuffer(1024, 1);
        server_data_out((*it)->get_client(), buffer);
    }
}

void modem::start_transmit() {
    std::cerr << "Radio command to transmit" << std::endl;
}

void modem::transmit(const void * data, size_t length) {
    std::cerr << "transmit " << length << " bytes" << std::endl;
}

#if 0
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "whitebox.h"
#include "modem.h"
#include "radio.h"
#include "dsp.h"

#define RF_SAMPLE_RATE 50000

#define SENSITIVITY ((int16_t)((2. * 3.1415926 * 5e3 / (RF_SAMPLE_RATE * 2.)) * 32767))

static struct whitebox wb;
static struct whitebox *whitebox;
static bool started = false;
static bool txing = false;
static bool rxing = false;
static char mode[5];
static uint32_t fcw;
static uint32_t phase;

typedef uint32_t (*modulator)(int16_t);
typedef int16_t (*demodulator)(uint32_t);

struct modulators_list {
    const char *name;
    modulator mod;
    demodulator demod;
};

modulator current_mod;
demodulator current_demod;

uint32_t am_mod(int16_t audio) {
    int16_t a = audio;// + 0x100;
    uint32_t iq_sample = QUAD_PACK(a, -a);
    return iq_sample;
}

#define MAG_ALPHA ((int32_t)(((1 << 15) - 1) * 0.948059448969))
#define MAG_BETA  ((int32_t)(((1 << 15) - 1) * 0.392699081699))

int16_t am_demod(uint32_t iq) {
    int16_t i, q;
    int32_t mAx, mIn;
    QUAD_UNPACK(iq, i, q);
    if (i > q) {
        mAx = i; mIn = q;
    } else {
        mAx = q; mIn = i;
    }
    return ((int16_t)((mAx * MAG_ALPHA) + (mIn * MAG_BETA)) >> 16);
}

uint32_t fm_mod(int16_t audio) {
    return 0;
}

int16_t fm_demod(uint32_t iq) {
    return 0;
}

uint32_t cw_mod(int16_t audio) {
    int16_t i, q;
    sincos16(fcw, &phase, &i, &q);
    return QUAD_PACK(i >> 3, -q >> 3);
}

int16_t cw_demod(uint32_t iq) {
    //int16_t i, q;
    //sincos16(fcw, &phase, &i, &q);
    //return i;
    return 0;
}

uint32_t usb_mod(int16_t audio) {
    return 0;
}

int16_t usb_demod(uint32_t iq) {
    return 0;
}

uint32_t lsb_mod(int16_t audio) {
    return 0;
}

int16_t lsb_demod(uint32_t iq) {
    return 0;
}

static const struct modulators_list modulators_list[] = {
    { "AM", am_mod, am_demod },
    { "FM", fm_mod, fm_demod },
    { "CW", cw_mod, cw_demod },
    { "USB", usb_mod, usb_demod },
    { "LSB", lsb_mod, lsb_demod },
    { 0, 0},
};


void *modem_init() {
    std::cerr << "Opening the modem";
    whitebox = &wb;
    /*whitebox = (struct whitebox*)malloc(sizeof(whitebox));
    if (!whitebox) {
        std::cerr << "Error: out of memory" << std::endl;
        return NULL;
    }*/
    //std::cerr << "Sensitivity" << SENSITIVITY << std::endl;
    whitebox_init(whitebox);
    whitebox->frequency = 145e6;
    fcw = freq_to_fcw(400, RF_SAMPLE_RATE);
    phase = 0;
    /*if (whitebox_open(whitebox, "/dev/whitebox", O_RDWR | O_NONBLOCK,
            RF_SAMPLE_RATE) < 0) {
        std::cerr << "Error: Couldn't open the whitebox" << std::endl;
        return NULL;
    }*/
    /*if (whitebox_mmap(whitebox) < 0) {
        std::cerr << "Error: couldn't mmap the whitebox" << std::endl;
        return NULL;
    }*/
    modem_set_mode("AM");
    whitebox_tx_set_latency(whitebox, 10);
    return whitebox;
}

void modem_close(void *data) {
    modem_standby();

    whitebox_munmap(whitebox);
    whitebox_close(whitebox);
    //free(whitebox);
}

int modem_descriptors_count(void *data) {
    return 0;
}

int modem_descriptors(void *data, struct pollfd *ufds, int count) {
    return 0;
}

void modem_handler(void *data, struct pollfd *ufds, int count) {
}

void modem_transmit() {
    rxing = false;
    if (whitebox_tx(whitebox, whitebox->frequency) < 0) {
        std::cerr << "Transmit start failed!" << std::endl;
        //exit(-1);
    }
    txing = true;

    if (!started) {
        std::cerr << "Transmit start (first time)" << std::endl;
        poll_start_fd(0, whitebox->fd, POLLOUT | POLLERR, WHITEBOX_FD);
        started = true;
    } else {
        std::cerr << "Transmit start" << std::endl;
        poll_change_fd(whitebox->fd, POLLOUT | POLLERR);
    }
}

void modem_receive() {
    long count;
    unsigned long src;
    txing = false;
    if (whitebox_rx(whitebox, whitebox->frequency) < 0) {
        std::cerr << "Receive start failed!" << std::endl;
        //exit(-1);
    }

    if (!started) {
        std::cerr << "Receive started (first time)" << std::endl;
        poll_start_fd(0, whitebox->fd, POLLIN | POLLERR, WHITEBOX_FD);
        started = true;
    } else {
        std::cerr << "Receive started" << std::endl;
        poll_change_fd(whitebox->fd, POLLIN | POLLERR);
    }
    rxing = true;
    // Crank the receiver to get things going.
    count = ioctl(whitebox->fd, W_MMAP_READ, &src) >> 2;

    // TODO: I need a modem receiver callback every 20ms
    
}

void modem_receiver_callback() {
    // Should call server_data_out
}

void modem_standby() {
#if 1
    if (txing) {
        if (whitebox_tx_standby(whitebox) < 0) {
            std::cerr << "Tx standby failed!" << std::endl;
            //exit(-1);
        }
    }
    if (rxing) {
        if (whitebox_rx_standby(whitebox) < 0) {
            std::cerr << "Rx standby failed!" << std::endl;
            //exit(-1);
        }
    }
    if (started) {
        //poll_change_fd(whitebox->fd, POLLERR);
        poll_end_fd(whitebox->fd);
        started = false;
    }
    txing = rxing = false;
#endif
}

void modem_write() {
    if (!txing) {
        std::cerr << "sending, but not txing?" << std::endl;
        //exit(-1);
    }
    unsigned long dest, count;
    count = ioctl(whitebox->fd, W_MMAP_WRITE, &dest) >> 2;
    count = count < 1024 ? count : 1024;
    if (count == 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        int16_t audio_sample = source();
        //std::cerr << ' ' << audio_sample;
        //fprintf(stderr, " %d", audio_sample);
        ((uint32_t*)dest)[i] = current_mod(audio_sample);
    }
    int ret = write(whitebox->fd, 0, count << 2);
    if (ret != count << 2) {
        std::cerr << "Write error" << std::endl;
        //exit(-1);
        //modem_recover();
    }
}

void modem_read() {
    if (!rxing) {
        std::cerr << "reading, but not rxing?" << std::endl;
        //exit(-1);
    }
    //std::cerr << "Modem read" << std::endl;
    unsigned long src, count;
    count = ioctl(whitebox->fd, W_MMAP_READ, &src) >> 2;
    count = count < 1024 ? count : 1024;
    if (count == 0) return;
    for (int i = 0; i < count; i++) {
        //int16_t audio_sample = source();
        //((uint32_t*)dest)[i] = current_mod(audio_sample);
        sink(current_demod(((uint32_t*)src)[i]));
    }
    int ret = read(whitebox->fd, 0, count << 2);
    if (ret != count << 2) {
        std::cerr << "Read error" << std::endl;
        //exit(-1);
        //modem_recover();
    }
}

void modem_recover() {
    //std::cerr << "recover" << std::endl;
    return;
    //whitebox_reset(whitebox);
    if (txing) {
        if (whitebox_tx(whitebox, whitebox->frequency) < 0) {
            std::cerr << "tx recover failed" << std::endl;
            //exit(-1);
        }
    }
    else if (rxing) {
        if (whitebox_rx(whitebox, whitebox->frequency) < 0) {
            std::cerr << "rx recover failed" << std::endl;
            //exit(-1);
        }
    } else {
    }
}

float modem_get_frequency() {
    return whitebox->frequency;
}

void modem_set_frequency(float frequency) {
    if (whitebox->frequency != frequency) {
        std::cerr << "new frequency " << frequency << std::endl;
        whitebox->frequency = frequency;
        if (txing) whitebox_tx_fine_tune(whitebox, frequency);
        if (rxing) whitebox_rx_fine_tune(whitebox, frequency);
    }
}

const char* modem_get_mode() {
    return mode;
}

void modem_set_mode(const char* new_mode) {
    if (strcmp(mode, new_mode) != 0) {
        std::cerr << "new mode " << new_mode << std::endl;
        strncpy(mode, new_mode, 4);
        mode[4] = '\0';  // No attacks!
    }
    for (int i = 0; modulators_list[i].name; ++i) {
        if (strcmp(modulators_list[i].name, mode) == 0) {
            current_mod = modulators_list[i].mod;
            current_demod = modulators_list[i].demod;
            return;
        }
    }
    std::cerr << "uknown mode" << std::endl;
    modem_set_mode("AM");
}

const bool modem_get_lna() {
    return whitebox->lna;
}

void modem_set_lna(const bool new_lna) {
    if (whitebox->lna != new_lna) {
        whitebox->lna = new_lna;
        if (rxing) {
            std::cerr  << "LNA" << new_lna << std::endl;
            whitebox_rx_config(whitebox);
        }
    }
}

const int modem_get_vga() {
    return whitebox->vga;
}

void modem_set_vga(const int vga) {
    if (whitebox->vga != vga) {
        whitebox->vga = vga;
        if (rxing) {
            std::cerr  << "vga" << vga;
            whitebox_rx_config(whitebox);
        }
    }
}

const char* _string_for_if_bw(if_filter_bw_t if_bw) {
    switch (whitebox->if_bw) {
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

const char* modem_get_if_bw() {
    return _string_for_if_bw(whitebox->if_bw);
}

void modem_set_if_bw(const char* if_bw) {
    if (strcmp(_string_for_if_bw(whitebox->if_bw), if_bw) != 0) {
        whitebox->if_bw = _enum_for_if_bw(if_bw);
        if (rxing) {
            std::cerr  << "if_bw" << if_bw << std::endl;
            whitebox_rx_config(whitebox);
        }
    }
}

const int modem_get_bpf() {
    return whitebox->bpf;
}

void modem_set_bpf(const int bpf) {
    if (bpf != whitebox->bpf) {
        whitebox->bpf = bpf;
        whitebox_gateway_set(whitebox);
    }
}

const float modem_get_rssi() {
    return 0.0; // TODO
}

const float modem_get_temp() {
    return 0.0; // TODO
}

const float modem_get_voltage() {
    return 0.0; // TODO
}

const bool modem_get_locked_status() {
    return whitebox_plls_locked(whitebox);
}

const bool modem_get_pa() {
    return whitebox->pa;
}

void modem_set_pa(const bool new_pa) {
    if (whitebox->pa != new_pa) {
        whitebox->pa = new_pa;
        if (txing) {
            std::cerr  << "PA" << new_pa << std::endl;
            whitebox_tx_config(whitebox);
        }
    }
}

const bool modem_get_led() {
    return whitebox->led;
}

void modem_set_led(const bool new_led) {
    if (whitebox->led != new_led) {
        whitebox->led = new_led;
        std::cerr << "LED" << new_led << std::endl;
        if (whitebox->led)
            whitebox_led_on();
        else
            whitebox_led_off();
    }
}

struct resource_ops modem_ops = {
    modem_init,
    modem_close,
    modem_descriptors_count,
    modem_descriptors,
    modem_handler,
};
#endif
