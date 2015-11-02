#ifndef __REPEATER_H__
#define __REPEATER_H__

#include <list>

class radio_context;

class repeater // Uses the RF Controller singleton from the HAL
{
    public:
        static repeater& get_instance()
        {
            static repeater instance;
            return instance;
        }

        void connect(radio_context *);
        void disconnect(radio_context *);

        void start_receive();
        void receive_cb(unsigned int type, const void * data, size_t length);

        void start_transmit(radio_context *);

    private:
        repeater() {
            transmitter = NULL;
        };

        repeater(repeater const&); // No copy
        void operator=(repeater const&); // No equals


        std::list<radio_context *> clients;

        radio_context * transmitter;
        bool try_transmit_lock(radio_context *);
};

#endif
