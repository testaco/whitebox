#ifndef __WHITEBOX_HTTP_H__
#define __WHITEBOX_HTTP_H__

#define HTTP_METHOD_LEN          7
#define HTTP_URL_LEN             255
#define HTTP_VERSION_LEN         31
#define HTTP_STATUS_MSG_LEN      31
#define HTTP_RESPONSE_TYPE_LEN   31
#define HTTP_RESPONSE_LEN        1023
#define HTTP_PARAM_NAME_LEN      63
#define HTTP_PARAM_VALUE_LEN     63

#define HTTP_PARAMS_MAX          32

#define HTTP_BUFFER_LEN          4095
#define HTTP_LINE_LEN            2047

struct http_request {
    char method[HTTP_METHOD_LEN + 1];
    char url[HTTP_URL_LEN + 1];
    char version[HTTP_VERSION_LEN + 1];

    struct {
        char name[HTTP_PARAM_NAME_LEN + 1];
        char value[HTTP_PARAM_VALUE_LEN + 1];
    } params[HTTP_PARAMS_MAX];

    int status_code;
    char status_msg[HTTP_STATUS_MSG_LEN + 1];
    char response_type[HTTP_RESPONSE_TYPE_LEN + 1];
    char response[HTTP_RESPONSE_LEN + 1];
    int response_len;
};

int http_parse(int fd, struct http_request *r);

int http_respond_file(int fd,
        struct http_request *r,
        char *response_type,
        char *filename);

int http_respond_string(int fd,
        struct http_request *r,
        char *response_type,
        char *response_fmt, ...);

int http_respond_error(int fd,
        struct http_request *r,
        int code);

#endif /* __WHITEBOX_HTTP_H__ */
