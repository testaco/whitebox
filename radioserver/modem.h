#ifndef __MODEM_H__
#define __MODEM_H__

#include <list>

class radio_context;
//#include "resources.h"

#if 0
class SignalGenerator // Signal Generator, exposes Iterator interface
{
    public:
        uint32_t next();
}


class SignalAnalyzer // Signal Analyzer, exposes Iterator interface
{
    public:
        uint32_t next();
}
#endif

class modulator {
    public:
        size_t space_available() { return 0; }
        // A single sample
        void modulate(uint32_t) {}
};

class demodulator {
    public:
        size_t data_available() { return 0; }
        // A single sample
        uint32_t demodulate() { return 0; }
};

enum modem_state {
    power_on,
    reset,
    self_test,
    idle,
    receive,
    receive_callback,
};

static const char * modem_state_strings[] = {
    "Power On",
    "Reset",
    "Self Test",
    "Idle",
    "Receive",
    "Receive Callback",
};

class modem // Uses the RF Controller singleton from the HAL
{
    public:
        static modem& get_instance()
        {
            static modem instance;
            return instance;
        }

        void connect(radio_context *);
        void disconnect(radio_context *);

        const char * get_state_string();
        const char* get_mode();
        void set_mode(const char* new_mode);

        void start_transmit();
        void transmit(const void * data, size_t length);

        void start_receive();

        int run_next_task();
        int run_task(modem_state state);

        //void schedule_task(modem_state next_state, int timeout);

        modulator * get_modulator() { return mod; }
        demodulator * get_demodulator() { return demod; }

    private:
        modem() {
            curr_state = idle;
            //modem_task = NULL;
            //modem_taskid = -1;
        };

        modem(modem const&); // No copy
        void operator=(modem const&); // No equals

        std::list<radio_context *> connections;

        void start();
        void standby();

        void receive_cb();
        void end_receive();
        void end_transmit();

        modulator * mod;
        demodulator * demod;

        //int modem_taskid;
        //task_handler *  modem_task;
        modem_state curr_state;
};

#endif /* __MODEM_H__ */
