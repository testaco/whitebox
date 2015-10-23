/*
 * Copyright (C) 2015 Algoram. Dual-licensed AGPL3 and commercial.
 * If your company has not purchased a commercial license from Algoram,
 * you are bound to the terms of the Affero GPL 3.
 */

//#include "../../libwebsockets/lib/private-libwebsockets.h"
#include <libwebsockets.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#if OPENSSL_FOUND
extern "C" {
#include <openssl/ssl.h>
}
#endif
#include "cJSON.h"
#include "radio.h"

#define PROGRAM_NAME "radioserver/"

#define DOUBLE_STRINGIZE(arg) #arg
#define STRINGIZE(arg) DOUBLE_STRINGIZE(arg)

class radio_context;

struct suffix_and_type {
  const char * const    suffix;
  const char * const    mime_type;
};

static const suffix_and_type type_table[] = {
  { "bin",      "application/octet-stream" },
  { "css",      "text/css; charset=utf-8" },
  { "csv",      "text/csv; charset=US-ASCII" },
  { "gz",       "multipart/x-gzip" },
  { "html",     "text/html; charset=utf-8" },
  { "ico",      "image/x-icon" },
  { "jpeg",     "image/jpeg" },
  { "js",       "application/ecmascript" },
  { "json",     "application/json" },
  { "manifest", "application/x-web-app-manifest+json" },
  { "mp3",      "audio/mpeg" },
  { "mp4",      "video/mp4" },
  { "png",      "image/png" },
  { "svg",      "image/svg+xml" },
  { "txt",      "text/plain; charset=US-ASCII;" },
  { "xml",      "application/xml; charset=utf-8" },
  { "zip",      "application/zip" },
  { 0, 0 }
};

struct client_context {
  radio_context *               radio;   // Opaque user-defined context.
  client_info			info;
  libwebsocket *                wsi;
  libwebsocket_context *	websocket_context;
  WriteBuffer *			buffers;
  WriteBuffer *			last_buffer;
  bool				open;
};

typedef int (*command_function)(
 const char *,
 cJSON *,
 client_context *);

struct command_processor {
  const char *          name;
  command_function      function;
};

#if OPENSSL_FOUND
static const int NID_callsign = OBJ_create("1.3.6.1.4.1.12348.1.1",
   "Callsign for identification in Logbook of the World",
   "Callsign");

static const int NID_dxccEntity = OBJ_create("1.3.6.1.4.1.12348.1.4",
   "Distant eXchange Century Club (DXCC) entity index value",
   "DXCC entity");
#endif

static int close(const char *, cJSON *, client_context *);
static int receive(const char *, cJSON *, client_context *);
static int set(const char *, cJSON *, client_context *);
static void send_status(client_context *);
static int transmit(const char *, cJSON *, client_context *);

static int
command(char * text, int length, client_context *);

static void
monitor_poll_fd(
 libwebsocket_context *         context,
 libwebsocket_callback_reasons  reason,
 void * in);

static const command_processor  commands[] = {
  { "close",    close },
  { "receive",  receive },
  { "transmit", transmit },
  { "set", set },
  { 0, 0 }
};

const size_t pad_size = LWS_SEND_BUFFER_PRE_PADDING + sizeof(uint32_t)
 + LWS_SEND_BUFFER_POST_PADDING;

WriteBuffer::WriteBuffer(size_t l, uint32_t _type)
: storage(new unsigned char[l + pad_size]),
  buf(&storage[LWS_SEND_BUFFER_PRE_PADDING + sizeof(uint32_t)]), next(0), maxLength(l), size(l)
{
  unsigned char * const	d = buf - sizeof(uint32_t);
  *(uint32_t *)d = _type;
}

WriteBuffer::~WriteBuffer()
{
  delete[] storage;
}

static void
get_client_info(libwebsocket_context * context, libwebsocket * wsi, client_context * client)
{
  char		buffer[256];
  char		rip[256];

#if OPENSSL_FOUND
  X509 *	certificate = SSL_get_peer_certificate(wsi->ssl);
  if ( certificate ) {
    client->info.certificate_is_valid = SSL_check_private_key(wsi->ssl);
    X509_NAME *subj = X509_get_subject_name(certificate);
    X509_NAME_get_text_by_NID(subj, NID_commonName, buffer, sizeof(buffer));
    client->info.name = strdup(buffer);
    buffer[0] = '\0';
    X509_NAME_get_text_by_NID(subj, NID_pkcs9_emailAddress, buffer, sizeof(buffer));
    client->info.email = strdup(buffer);
    buffer[0] = '\0';
    X509_NAME_get_text_by_NID(subj, NID_callsign, buffer, sizeof(buffer));
    client->info.callsign = strdup(buffer);
    buffer[0] = '\0';
    rip[0] = '\0';
  }
  else {
    client->info.certificate_is_valid = false;
  }
#else
  client->info.certificate_is_valid = false;
#endif
  libwebsockets_get_peer_addresses(context, wsi, libwebsocket_get_socket_fd(wsi), buffer, sizeof(buffer), rip, sizeof(rip));
  client->info.hostname = strdup(buffer);
  client->info.ip_address = strdup(rip);
}

#if 0
static void
dump_headers(libwebsocket_context * context, libwebsocket * wsi)
{
  for ( int n = 0; const unsigned char * c = lws_token_to_string((lws_token_indexes)n); n++ ) {

    if ( lws_hdr_total_length(wsi, (lws_token_indexes)n) > 0 ) {
       char	buf[256];
       lws_hdr_copy(wsi, buf, sizeof(buf), (lws_token_indexes)n);
       std::cerr << c << ' ' << buf << std::endl;
    }
  } 
  std::cerr << std::endl;
}
#endif

static void
get_headers(libwebsocket_context * context, libwebsocket * wsi, client_context * client)
{
  char	value[256];

  // Get the path string.
  value[0] = '\0';
  lws_hdr_copy(wsi, value, sizeof(value), WSI_TOKEN_GET_URI);
  client->info.path = strdup(value);
  value[0] = '\0';
  lws_hdr_copy(wsi, value, sizeof(value), WSI_TOKEN_HTTP_USER_AGENT);
  client->info.user_agent = strdup(value);
}

static int
serve_string(
 libwebsocket_context *         context,
 libwebsocket *                 wsi,
 const char *			s,
 const char *			content_type)
{
  unsigned char 		buf[2048];
  const long unsigned int		length = strlen(s);
  unsigned char * 		p = buf;
  unsigned char * const		end = &buf[sizeof(buf) - 1];

  // This gets us a reload of the dynamic script in response to the browser
  // history back button.
  static const char		cache_advice[] = "no-cache, no-store";

  lws_add_http_header_status(context, wsi, 200, &p, end);
  lws_add_http_header_by_token(
   context,
   wsi,
   WSI_TOKEN_HTTP_CONTENT_TYPE,
   (const unsigned char *)content_type,
   strlen(content_type),
   &p,
   end
  );
  lws_add_http_header_by_token(
   context,
   wsi,
   WSI_TOKEN_HTTP_CACHE_CONTROL,
   (const unsigned char *)cache_advice,
   sizeof(cache_advice) - 1,
   &p,
   end
  );
  lws_add_http_header_content_length(context, wsi, length, &p, end);
  lws_finalize_http_header(context, wsi, &p, end);
  memcpy(p, s, length);
  p += length;
  libwebsocket_write(wsi, buf, (size_t)(p - buf), LWS_WRITE_HTTP);
  if (  lws_http_transaction_completed(wsi) )
    return -1;
  else
    return 0;
}

static std::ostream &
emit(std::ostream & stream, const char * name, const char * value)
{
  if ( value == 0 )
    return stream;
  else
    return stream << "\t" << name << ": \"" << value << "\"," << std::endl;
}

int
serve_client_info_dot_js(
 libwebsocket_context *         context,
 libwebsocket *                 wsi,
 void *				user
)
{
  client_context * const client = (client_context *)user; 

  get_client_info(context, wsi, client);
  const client_info * const i = &client->info;

  std::stringstream str;
  str << "window.client_info = {" << std::endl;
  str << "\tcertificate_is_valid: " << (i->certificate_is_valid ? "true" : "false") << "," << std::endl;
  emit(str, "callsign", i->callsign);
  emit(str, "name", i->name);
  emit(str, "email", i->email);
  emit(str, "hostname", i->hostname);
  emit(str, "ip_address", i->ip_address);
  emit(str, "user_agent", i->user_agent);
  str << "};" << std::endl;

  return serve_string(
   context,
   wsi,
   str.str().c_str(),
   "application/ecmascript"
  );
}

// If the software has been updated and there are still old HTML files in the web
// server cache, force a reload and history substitution.
// Each HTML file includes the script version.js, with the argument "?version=x.y.z&".
//
// If the version number argument matches that of the server, send an empty script.
// This is the usual case and it's fast.
//
// If the version numbers don't match those of the server, send a script that will
// execute immediately and cause the page to be reloaded.
//
// Detect if a reload has happened, and if so don't reload again. Thus preventing loops
// if the version number of the HTML file doesn't match that of the server.
//
// Cache control statements are sent with the file, to cause reloading of version.js
// if we revisit a page from the browser history.
int
serve_version_dot_js(
 libwebsocket_context *         context,
 libwebsocket *                 wsi
)
{
#define	VERSION_STRING \
 STRINGIZE(MAJOR_VERSION) "." STRINGIZE(MINOR_VERSION) "." STRINGIZE(PATCH_VERSION)

  static const char version_argument[] = "version=" VERSION_STRING "&";
  char value[128];

  value[0] = '\0';
  lws_hdr_copy(wsi, value, sizeof(value), WSI_TOKEN_HTTP_URI_ARGS);
  if ( strcmp(value, version_argument) == 0 ) {
    // Send an empty file.
    return serve_string(context, wsi, "", "application/ecmascript");
  }
  else {
    std::cerr << "Caught HTML version mismatch, forcing reload." << std::endl;
    // Send a script that will cause the page to be reloaded.
    static const char program[] =
     "const version_argument =\"version=" VERSION_STRING "&\";"
     "console.log(\"Detected software update, reloading this page.\");"
     "if (!window.location.href.match(version_argument)){"
     "window.location.href="
     "window.location.href.replace(/(\\?.*$|$)/, \"?\" + version_argument);}";

    return serve_string(
     context,
     wsi,
     program,
     "application/ecmascript"
    );
  }
}

static int serve_dynamic_http(
 libwebsocket_context *          context,
 libwebsocket *                  wsi,
 libwebsocket_callback_reasons   /* reason */,
 void *                          user,
 void *                          in,
 size_t                          /* len */)
{

  if ( strcmp((char *)in, "/dynamic/version.js") == 0 )
    serve_version_dot_js(context, wsi);
  else if ( strcmp((char *)in, "/dynamic/client_info.js") == 0 )
    serve_client_info_dot_js(context, wsi, user);
  else {
    libwebsockets_return_http_status(
     context,
     wsi,
     404,
     "Not found."
    );
    return -1;
  }
  if ( lws_http_transaction_completed(wsi) )
    return -1;
  else
    return 0;
}

static int callback_http(
 libwebsocket_context *          context,
 libwebsocket *                  wsi,
 libwebsocket_callback_reasons   reason,
 void *                          user,
 void *                          in,
 size_t                          len)
{
  client_context * const client = (client_context *)user; 

  switch (reason) {
    case LWS_CALLBACK_GET_THREAD_ID:
      return pthread_self();
    case LWS_CALLBACK_LOCK_POLL:
    case LWS_CALLBACK_UNLOCK_POLL:
    case LWS_CALLBACK_ADD_POLL_FD:
    case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
    case LWS_CALLBACK_DEL_POLL_FD:
      monitor_poll_fd(context, reason, in);
      return 0;

    case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
      get_headers(context, wsi, client);
      return 0;

    case LWS_CALLBACK_HTTP:
      {
        static const char	cache_advice[] =
         "Cache-Control: private, max-age=604800\r\n";
	static const char       dirname[] = LIB_DIR PROGRAM_NAME \
				 "web_content/";
        static const char	dynamic_path[] = "dynamic/";
	const char *            i = (char *)in;
	const size_t            length = strlen(i);
	const char *            mime_type = "text/plain";
	char                    name[128];
	struct stat             filestat;
	int                     stat_ret;

	if ( *i == '/' )
	  i++;

        if ( strncmp(i, dynamic_path, sizeof(dynamic_path) - 1) == 0 )
	  return serve_dynamic_http(
           context,
	   wsi,
	   reason,
	   user,
	   in,
	   len);

	if ( strstr(i, "/../") 
	 ||  strcmp(i, "..") == 0
	 ||  strncmp(i, "../", 3) == 0
	 ||  (length > 3 && strncmp(&i[length - 4], "/..", 3) == 0)
	 ||  length > (sizeof(name) - (sizeof(dirname) - 1))
	 ||  length < 1 ) {
	  libwebsockets_return_http_status(
	   context,
	   wsi,
	   404,
	   "Not found.");
          return -1;
	}

	static const char index[] = "index.html";
	strcpy(name, dirname);
	strcat(name, i);

	const size_t long_length = strlen(name);

	stat_ret = stat(name, &filestat);

	if ( stat_ret == 0 ) {

	  if ( S_ISDIR(filestat.st_mode)
	   && name[long_length - 1] != '/' ) {
	    // We're asked for a directory without the trailing slash.
	    // Redirect, adding the trailing slash.

	    unsigned char buf[1024];
	    unsigned char * p = buf;
	    unsigned char * const end = &buf[sizeof(buf) - 1];

	    strcpy(name, i);
	    name[length] = '/';
	    name[length + 1] = '\0';

	    lws_add_http_header_status(context, wsi, 301, &p, end);
	    lws_add_http_header_by_token(
	     context,
	     wsi,
	     WSI_TOKEN_HTTP_LOCATION,
	     (const unsigned char *)name,
	     length + 1,
	     &p,
	     end);
    
	    lws_finalize_http_header(context, wsi, &p, end);
	    libwebsocket_write(wsi, buf, (size_t)(p - buf), LWS_WRITE_HTTP);
            break;
	  }
	}
	else {
	  libwebsockets_return_http_status(
	   context,
	   wsi,
	   404,
	   "Not found.");
          return -1;
	}

	if ( name[long_length - 1] == '/'
	 && long_length < sizeof(name) - sizeof(index) ) {
	  strcat(name, index);
	  mime_type = "text/html";
	}

	const char * dot = rindex(i, '.');
	if ( dot ) {
	  dot++;
	  const suffix_and_type * t = type_table;
	  while ( t->suffix != 0 ) {
	    if ( strcmp(dot, t->suffix) == 0 ) {
	      mime_type = t->mime_type;
	      break;
	    }
	    t++;
	  }
	}

	int status = libwebsockets_serve_http_file(
	 context,
	 wsi,
	 name,
	 mime_type,
	 cache_advice,
	 sizeof(cache_advice) - 1);

        if ( status == 0 )
          return 0;
        else
          break;
      }

#if OPENSSL_FOUND
    case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
      {
        SSL_CTX * ssl_context = (SSL_CTX *)user;
        // SSL_CTX_set_session_cache_mode(ssl_context, SSL_SESS_CACHE_OFF);
	if ( SSL_CTX_load_verify_locations(ssl_context, ETC_DIR PROGRAM_NAME \
         "/certificates/authority/default.pem", 0) != 1 ) {
          std::cerr << "Loading certification authority certificates failed." << std::endl;
        }
      }
      return 0;
#endif

    default:
      return 0;
  }
  if ( lws_http_transaction_completed(wsi) )
    return -1;
  else
    return 0;
}

static int callback_radioserver(
 libwebsocket_context *            context,
 libwebsocket *                    wsi,
 libwebsocket_callback_reasons     reason,
 void *                            user,
 void *                            in,
 size_t                            len)
{
  client_context * const client = (client_context *)user; 

  switch (reason) {
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
      get_headers(context, wsi, client);
      break;

    case LWS_CALLBACK_ESTABLISHED:
      {
        get_client_info(context, wsi, client);
        client->wsi = wsi;
        client->websocket_context = context;

        client->radio = radio_start(client, &client->info);
        client->open = true;
        send_status(client);
      }
      break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
      if ( client->open ) {
        WriteBuffer * b = client->buffers;
        while ( b ) {
          WriteBuffer * const old = b;
          unsigned char * const	d = b->data() - sizeof(uint32_t);

          libwebsocket_write(wsi, d, b->length(), LWS_WRITE_BINARY);
          b = b->link();
          delete old;
 
          if ( b && (lws_partial_buffered(client->wsi) || lws_send_pipe_choked(client->wsi))){
            client->buffers = b;
            libwebsocket_callback_on_writable(client->websocket_context, client->wsi);
            return 0;
          }
        }
        client->buffers = 0;
        client->last_buffer = 0;
      }
      break;

    case LWS_CALLBACK_RECEIVE:
      if ( client->open ) {
        if ( lws_frame_is_binary(wsi) ) {
          uint32_t * i = (uint32_t *)in; 
  	  radio_data_in(client->radio, &client->info, *i, (unsigned char *)&i[1], len - sizeof(uint32_t));
        }
        else
  	  return command((char *)in, len, client);
      }

      break;

    case LWS_CALLBACK_CLOSED:
      // free(client->path);
      if ( client->open ) {
        radio_end(client->radio, &client->info);
        client->radio = 0;
        client->open = false;
      }
      break;

    default:
      ;
  }
  return 0;
}

static int
close(const char *, cJSON *, client_context * client)
{
  radio_end(client->radio, &client->info);
  client->radio = 0;
  client->open = false;

  // This return value causes the server to disconnect.
  return 1;
}

static int
command(
 char *                         text,
 int                            /* length */,
 client_context *               client)
{
  const command_processor * c = commands;

  cJSON * const json = cJSON_Parse(text);
  if ( json == NULL ) {
    std::cerr << "JSON could not parse command \"" << text << "\"" << std::endl;
    return 1;
  }
  const char * const command = cJSON_GetObjectItem(json,"command")->valuestring;
  int value = 0;

  std::cerr << "Command: " << command << std::endl;
  while ( c->name != 0 ) {
    if ( strcmp(command, c->name) == 0 ) {
      value = c->function(command, json, client);
      break;
    }
    c++;
  }
  cJSON_Delete(json);
  return value;
}

void
server_poll_handler(pollfd *fds, void *data)
{
  struct libwebsocket_context *context = (libwebsocket_context *)data;
  server_service_fd(context, fds);
}

static void
monitor_poll_fd(
 libwebsocket_context *         context,
 libwebsocket_callback_reasons  reason,
 void *                         in)
{
  pollfd * args = (pollfd *)in;

  switch ( reason ) {
  case LWS_CALLBACK_LOCK_POLL:
    return;
  case LWS_CALLBACK_UNLOCK_POLL:
    return;
  case LWS_CALLBACK_ADD_POLL_FD:
    poll_start_fd(args->fd, args->events, server_poll_handler, context);
    break;
  case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
    poll_change_fd(args->fd, args->events);
    break;
  case LWS_CALLBACK_DEL_POLL_FD:
    poll_end_fd(args->fd);
    break;
  default:
    ;
  }
}

static int
receive(const char *, cJSON *, client_context * client)
{
  radio_receive(client->radio, &client->info);
  send_status(client);
  return 0;
}

void
server_end(libwebsocket_context * context)
{
  libwebsocket_context_destroy(context);
}

void
server_data_out(client_context * client, WriteBuffer * buffer)
{
  if ( client->last_buffer ) {
    client->last_buffer->link(buffer);
    client->last_buffer = buffer;
  }
  else {
    client->buffers = client->last_buffer = buffer;
  }

  libwebsocket_callback_on_writable(client->websocket_context, client->wsi);
}

void
server_service_fd(libwebsocket_context * context, pollfd * pfd)
{
  libwebsocket_service_fd(context, pfd);
}

libwebsocket_context *
server_start(const char * device, int port, bool use_ssl)
{
  static const libwebsocket_protocols protocols[] = {
    {
      "http-only",      // Protocol name.
      callback_http,    // Handler function.
      sizeof(client_context),// Size of user-private per-session data.
      10240,            // Maximum supported receive buffer size.       
      0,                // User-private protocol ID
      0, 0, 0           // libwebsockets will set these fields.
    },
    {
      "radio-server-1",         // Protocol name.
      callback_radioserver,     // Handler function.
      sizeof(client_context),   // Size of user-private per-session data.
      10240,                    // Maximum supported receive buffer size.       
      0,                        // User-private protocol ID
      0, 0, 0                   // libwebsockets will set these fields.
    },
    { 0, 0, 0, 0, 0, 0, 0, 0 }
  };

  lws_context_creation_info http_port;

  libwebsocket_protocols * p = new libwebsocket_protocols[sizeof(protocols)/sizeof(*protocols)];
  memcpy(p, protocols, sizeof(protocols));
  memset(&http_port, 0, sizeof(http_port));
  http_port.port = port;
  http_port.iface = device;
  http_port.protocols = p;
  http_port.extensions = 0;
  http_port.token_limits = 0;
  http_port.provided_client_ssl_ctx = 0;
  http_port.uid = -1;
  http_port.gid = -1;
  http_port.user = 0;
  http_port.options = 0;

  // TCP keepalive settings to close a dead connection after 20 minutes.
  http_port.ka_time = 30;   // Start sending keepalives if idle for this long.
  http_port.ka_interval = 5; // Then send them at this interval.
  http_port.ka_probes = 10;  // Close the connection after this many fail.
  
  lws_set_log_level(0, 0);
  // Valid log levels are:
  // lws_set_log_level(LLL_ERR|LLL_WARN|LLL_NOTICE|LLL_INFO|LLL_DEBUG|LLL_PARSER|LLL_HEADER|LLL_EXT|LLL_CLIENT|LLL_LATENCY, 0);

  if ( use_ssl ) {
    http_port.options = LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
    http_port.ssl_cert_filepath = ETC_DIR PROGRAM_NAME \
     "/certificates/public/default.pem";
    http_port.ssl_private_key_filepath = ETC_DIR PROGRAM_NAME \
     "/certificates/private/default.key";

    // The CA chain is loaded in response to
    // LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS,
    // not here, because setting this field to load them doesn't set them for a
    // server to verify clients.
    http_port.ssl_ca_filepath = 0;
    http_port.ssl_cipher_list = 0;
  }

  return libwebsocket_create_context(&http_port);
}

static int
set(
 const char *           , // command
 cJSON *                json,
 client_context *       client)
{
  radio_set(client->radio, &client->info, json);
  send_status(client);
  return 0;
}

// Send the current status of the radio to the client.
static void
send_status(client_context * client)
{

  cJSON * const json = cJSON_CreateObject();    
  radio_get_status(client->radio, &client->info, json);
  char * text = cJSON_Print(json);
  WriteBuffer * buffer = new WriteBuffer(strlen(text), 0);
  strcpy((char *)buffer->data(), text);
  free(text);
  cJSON_Delete(json);
  server_data_out(client, buffer);
}

static int
transmit(
 const char *,
 cJSON *,
 client_context * client)
{
  radio_transmit(client->radio, &client->info);
  send_status(client);
  return 0;
}
