/*
 * Copyright (C) 2015 Algoram. Dual-licensed AGPL3 and commercial.
 * If your company has not purchased a commercial license from Algoram,
 * you are bound to the terms of the Affero GPL 3.
 */

#include <libwebsockets.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <cerrno>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "cJSON.h"
#include "radio.h"

#define ETC_DIR "/etc/"
#define LIB_DIR "/usr/lib/"
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
  libwebsocket_context *        websocket_context;
  libwebsocket *                wsi;
  bool				open;
  const char *                  path;
};

typedef int (*command_function)(
 const char *, cJSON *,
 client_context *);

struct command_processor {
  const char *          name;
  command_function      function;
};

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

static int
serve_string(
 libwebsocket_context *         context,
 libwebsocket *                 wsi,
 const char *			s,
 const char *			content_type)
{
  unsigned char 	buf[2048];
  const int		length = strlen(s);
  unsigned char * 	p = buf;
  unsigned char * const	end = &buf[sizeof(buf) - 1];
  // This gets us a reload of the dynamic script in response to the browser
  // history back button.
  static const char	cache_advice[] =
   "Cache-Control: private, no-store, max-age=0, no-cache, must-revalidate";

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
  libwebsocket_write(wsi, buf, p - buf, LWS_WRITE_HTTP);
  return lws_http_transaction_completed(wsi);
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
 STRINGIZE(0) "." STRINGIZE(1) "." STRINGIZE(1)

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
 void *                          /* user */,
 void *                          in,
 size_t                          /* len */)
{

  if ( strcmp((char *)in, "/dynamic/version.js") == 0 )
    return serve_version_dot_js(context, wsi);
  else {
    libwebsockets_return_http_status(
     context,
     wsi,
     404,
     "Not found."
    );
  }
  return lws_http_transaction_completed(wsi);
}

static int callback_http(
 libwebsocket_context *          context,
 libwebsocket *                  wsi,
 libwebsocket_callback_reasons   reason,
 void *                          user,
 void *                          in,
 size_t                          len)
{
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
          break;
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
	    // I don't think it's possible for us to have a partial buffer here, but
            // if it is this code gets more complicated.
	    libwebsocket_write(wsi, buf, p - buf, LWS_WRITE_HTTP);

            break;
	  }
	}
	else {
	  libwebsockets_return_http_status(
	   context,
	   wsi,
	   404,
	   "Not found.");
          break;
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

    case LWS_CALLBACK_HTTP_FILE_COMPLETION:
      return 0;

    case LWS_CALLBACK_CLOSED_HTTP:
    case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
    case LWS_CALLBACK_PROTOCOL_DESTROY:
    case LWS_CALLBACK_PROTOCOL_INIT:
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
    case LWS_CALLBACK_WSI_CREATE:
    case LWS_CALLBACK_WSI_DESTROY:
    default:
      return 0;
  }
  return lws_http_transaction_completed(wsi);
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
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
      break;

    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
      {
        char			value[128];

	// Get the path string.
        value[0] = '\0';
	lws_hdr_copy(wsi, value, sizeof(value), WSI_TOKEN_GET_URI);
        client->path = strdup(value);
      }
      break;

    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
      break;

    case LWS_CALLBACK_ESTABLISHED:
      client->websocket_context = context;
      client->wsi = wsi;
      client->radio = radio_start(client);
      client->open = true;
      send_status(client);
      break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
      break;

    case LWS_CALLBACK_RECEIVE:
      if ( lws_frame_is_binary(wsi) )
	radio_data_in(client->radio, 0, in, len);
      else
	return command((char *)in, len, client);

      break;

    case LWS_CALLBACK_CLOSED:
      free((void *)client->path);
      if ( client->open )
        radio_end(client->radio);
      break;

    case LWS_CALLBACK_PROTOCOL_INIT:
    case LWS_CALLBACK_PROTOCOL_DESTROY:
    default:
      ;
  }
  return 0;
}

static int
close(const char *, cJSON *, client_context * client)
{
  radio_end(client->radio);
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
    poll_start_fd(context, args->fd, args->events);
    break;
  case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
    poll_change_fd(args->fd, args->events);
    break;
  case LWS_CALLBACK_DEL_POLL_FD:
    //std::cerr << "****ENDING FD " << args->fd << std::endl;
    poll_end_fd(args->fd);
    break;
  default:
    ;
  }
}

static int
receive(const char *, cJSON *, client_context * client)
{
  radio_receive(client->radio);
  send_status(client);
  return 0;
}

void
server_end(client_context * client)
{
  libwebsocket_context_destroy(client->websocket_context);
}

void
server_data_out(client_context * client, unsigned char /* type */, void * data, int length)
{
  unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 10240 + LWS_SEND_BUFFER_POST_PADDING]; 
  unsigned char * const start = &buf[LWS_SEND_BUFFER_PRE_PADDING];
  memcpy(start, data, length);
  if ( lws_partial_buffered(client->wsi) )
    {}; // FIX: Bail out and retry later.
  libwebsocket_write(client->wsi, start, length, LWS_WRITE_BINARY);
}

void
server_service_fd(libwebsocket_context * context, pollfd * pfd)
{
  libwebsocket_service_fd(context, pfd);
}

bool
server_start()
{
  static libwebsocket_protocols protocols[] = {
    {
      "http-only",      // Protocol name.
      callback_http,    // Handler function.
      0,                // Size of user-private per-session data.
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

  memset(&http_port, 0, sizeof(http_port));
  http_port.port = 80;
  http_port.iface = 0;
  http_port.protocols = protocols;
  http_port.extensions = 0;
  http_port.token_limits = 0;
  http_port.provided_client_ssl_ctx = 0;
  http_port.uid = -1;
  http_port.gid = -1;
  http_port.options = 0;
  http_port.user = 0;

  // TCP keepalive settings to close a dead connection after 20 minutes.
  http_port.ka_time = 600;   // Start sending keepalives if idle for this long.
  http_port.ka_interval = 5; // Then send them at this interval.
  http_port.ka_probes = 10;  // Close the connection after this many fail.
  
  lws_set_log_level(0, 0);
  // Valid log levels are:
  // LLL_ERR|LLL_WARN|LLL_NOTICE|LLL_INFO|LLL_DEBUG|LLL_PARSER|LLL_HEADER
  //  |LLL_EXT|LLL_CLIENT|LLL_LATENCY

  libwebsocket_context * const http_context = libwebsocket_create_context(&http_port);

  if (http_context == NULL) {
    std::cerr << "Failed to start the http server: " << strerror(errno)
     << '.' << std::endl;
    return false;
  }

#ifdef LWS_OPENSSL_SUPPORT
  lws_context_creation_info https_port = http_port;

  https_port.port = 443;

  https_port.ssl_cert_filepath = ETC_DIR PROGRAM_NAME \
   "/certificates/public/default.pem";
  https_port.ssl_private_key_filepath = ETC_DIR PROGRAM_NAME \
   "/certificates/private/default.key";
  https_port.ssl_ca_filepath = ETC_DIR PROGRAM_NAME \
   "/certificates/authority/default.pem";
  https_port.ssl_cipher_list = 0;

    
  libwebsocket_context * const https_context = libwebsocket_create_context(&https_port);
  if (https_context == NULL) {
    std::cerr << "Failed to start the https server: " << strerror(errno)
     << '.' << std::endl;
    return false;
  }
#endif

  return true;
}

static int
set(
 const char *           , // command
 cJSON *                json,
 client_context *       client)
{
  radio_set(client->radio, json);
  send_status(client);
  return 0;
}

// Send the current status of the radio to the client.
static void
send_status(client_context * client)
{
  unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 2048 + LWS_SEND_BUFFER_POST_PADDING]; 
  unsigned char * const start = &buf[LWS_SEND_BUFFER_PRE_PADDING];

  cJSON * const json = cJSON_CreateObject();    
  radio_get_status(client->radio, json);
  const char * text = (const char *)cJSON_Print(json);
  strcpy((char *)start, text);
  if ( lws_partial_buffered(client->wsi) )
    {}; // FIX: Bail out and retry later.
  libwebsocket_write(client->wsi, start, strlen((const char *)start), LWS_WRITE_TEXT);
  cJSON_Delete(json);
}

static int
transmit(
 const char *,
 cJSON *,
 client_context * client)
{
  radio_transmit(client->radio);
  send_status(client);
  return 0;
}
