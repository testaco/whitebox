#ifndef __RADIO_CONTEXT__
#define __RADIO_CONTEXT__

// Add members to radio_context as you wish.
class radio_context {
private:

  radio_context(const radio_context &);
  radio_context &	operator =(const radio_context &);
  client_context * const client;
  const client_info * const info;

public:
    radio_context(client_context * c, const client_info * i)
        : client(c), info(i) { };
    ~radio_context() {}

    client_context * get_client() { return client; }
};

#endif
