class cJSON;

// These structures are known in the server code and
// opaque from the perspective of the radio code.
struct libwebsocket_context;
struct client_context;

// This class is known in the radio code and opaque
// from the perspective of the server code.
class radio_context;

#define WEBSOCKET_FD 0
#define WHITEBOX_FD 1
#define FILE_SOURCE_FD 2

extern void	poll_start_fd(libwebsocket_context *, int fd, int events, int type=0);
extern void	poll_change_fd(int fd, int mode);
extern void	poll_end_fd(int fd);

extern void		radio_end(radio_context *);
extern void		radio_get_status(radio_context *, cJSON * json);
extern radio_context *	radio_start(client_context * opaque);
extern void		radio_receive(radio_context *);
extern void		radio_set(radio_context *, const cJSON * json);
extern void		radio_transmit(radio_context *);

extern void		radio_data_in(
 radio_context *	context,
 unsigned char *	type,
 const void *		data,
 int			length);

void		server_end();

void		server_data_out(
                 client_context *	opaque,
		 unsigned char		type,
                 void *			data,
		 void *			length);

void		server_service_fd(libwebsocket_context * /* opaque */, pollfd * pfd);
bool		server_start();
