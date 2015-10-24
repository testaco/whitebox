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

class modem_connection;
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

        const char* get_mode();
        void set_mode(const char* new_mode);

        void start_transmit();
        void set_signal_generator(const char *name);
        void transmit(const void * data, size_t length);

        void start_receive();
        void set_signal_analyzer(const char *name);

    private:
        modem() {
        };

        modem(modem const&); // No copy
        void operator=(modem const&); // No equals

        std::list<radio_context *> connections;

        void standby();

        void modem_receive_callback();

    #if 0
        SignalGenerator *signal_generator;
        SignalAnalyzer *signal_analyzer;
    #endif
};

#endif /* __MODEM_H__ */
