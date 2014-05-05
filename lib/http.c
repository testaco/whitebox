#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "unistd.h"
#include "http.h"

int http_parse_status(struct http_request *r, char *line)
{
    char *method, *url, *version;
    method = strtok(line, " \t\r\n");
    url = strtok(NULL, " \t\r\n");
    version = strtok(NULL, " \t\r\n");

    strncpy(r->method, method, HTTP_METHOD_LEN);
    r->method[HTTP_METHOD_LEN] = '\0';
    strncpy(r->url, url, HTTP_URL_LEN);
    r->url[HTTP_URL_LEN] = '\0';
    strncpy(r->version, version, HTTP_VERSION_LEN);
    r->url[HTTP_VERSION_LEN] = '\0';

    //printf("status: %s %s %s\n", r->method, r->url, r->version);
    return 0;
}

int http_parse_header(struct http_request *r, char *line)
{
    char *var, *val;
    var = strtok(line, ": \t");
    val = strtok(NULL, "\r\n");
    //printf("header: %s %s\n", var, val);
    return 0;
}

int http_parse_body(struct http_request *r, char *body)
{
    char *var, *val;
    int i = 0;
    var = strtok(body, "=&\r\n");
    val = strtok(NULL, "=&\r\n");
    while (var && val && i < HTTP_PARAMS_MAX - 1) {
        strncpy(r->params[i].name, var, HTTP_PARAM_NAME_LEN);
        r->params[i].name[HTTP_PARAM_NAME_LEN] = '\0';
        strncpy(r->params[i].value, val, HTTP_PARAM_VALUE_LEN);
        r->params[i].value[HTTP_PARAM_VALUE_LEN] = '\0';
        var = strtok(NULL, "=&\r\n");
        val = strtok(NULL, "=&\r\n");
        i++;
    }
    r->params[i].name[0] = '\0';
    r->params[i].value[0] = '\0';
    return 0;
}

int http_parse(int fd, struct http_request *r)
{
    int recvd;
    static char command[HTTP_BUFFER_LEN + 1];
    char *i;
    int newline_machine = 0;
    char *linestart;
    char *bodystart;
    static char linebuf[HTTP_LINE_LEN + 1];
    int linenum;

    if ((recvd = read(fd, (void*)command, HTTP_BUFFER_LEN)) < 0) {
        if (errno == EAGAIN) {
            printf("Eagain\n");
            return recvd;
        }
        perror("recvfrom");
        exit(1);
    }

    if ((recvd == 1 && *((char*)command) == '\0') || (recvd == 0)) {
        return 0;
    }

    command[HTTP_BUFFER_LEN] = '\0';

    linenum = 0;
    linestart = command;
    for (i = command; *i; ++i) {
        switch (newline_machine) {
            case 0: case 2: {
                if (*i == '\r') newline_machine++;
                else newline_machine = 0;
            } break;
            case 1: {
                if (*i == '\n') newline_machine++;
                else newline_machine = 0;
            } break;
            case 3: {
                if (*i == '\n') newline_machine++;
                else newline_machine = 0;
            } break;
            default: {
                newline_machine = 0;
            }
        }
        if (newline_machine == 2) {
            int linelength = i - linestart;
            linelength = linelength < HTTP_LINE_LEN ? linelength : HTTP_LINE_LEN;
            strncpy(linebuf, linestart, i - linestart);
            linebuf[HTTP_LINE_LEN] = '\0';
            if (linenum == 0) {
                if (http_parse_status(r, linebuf) < 0) {
                    printf("Estatus\n");
                    return -1;
                }
            } else {
                if (http_parse_header(r, linebuf) < 0) {
                    printf("Eheader\n");
                    return -1;
                }
            }

            linestart = i + 1;
            linenum++;
        }
        if (newline_machine == 4) {
            bodystart = i + 1;
            if (http_parse_body(r, bodystart) < 0) {
                printf("Ebody\n");
                return -1;
            }
            linenum++;
            break;
        }
    }

    return 0;
}

int http_send_header(int fd, struct http_request *r)
{
    static char header[HTTP_LINE_LEN + 1];
    int header_len;

    header_len = snprintf(header, HTTP_LINE_LEN, 
        "%s %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        r->version,
        r->status_code,
        r->status_msg,
        r->response_type,
        r->response_len);
    return send(fd, header, header_len, 0);
}

int http_respond_file(int fd,
        struct http_request *r,
        char *response_type,
        char *filename)
{
    struct stat st;
    int file_fd = open(filename, O_RDONLY);

    if (file_fd < 0)
        return http_respond_error(fd, r, 404);

    if (fstat(file_fd, &st) < 0)
        return http_respond_error(fd, r, 404);

    r->response_len = st.st_size;
    r->status_code = 200;
    strcpy(r->status_msg, "OK");
    strcpy(r->response_type, response_type);

    if (http_send_header(fd, r) < 0) {
        close(file_fd);
        return -1;
    }

    sendfile(fd, file_fd, 0, r->response_len, 0);
    close(file_fd);
    return 0;
}

int http_respond_error(int fd, struct http_request *r, int code)
{
    r->response_len = 0;
    r->status_code = code;
    strcpy(r->status_msg, "NO");
    strcpy(r->response_type, "text/html");

    return http_send_header(fd, r);
}

int http_respond_string(int fd,
        struct http_request *r,
        char *response_type,
        char *response_fmt, ...)
{
    va_list arglist;

    va_start(arglist, response_fmt);
    vsnprintf(r->response, HTTP_RESPONSE_LEN, response_fmt, arglist);
    va_end(arglist);
    r->response_len = strlen(r->response);

    r->status_code = 200;
    strcpy(r->status_msg, "OK");
    strcpy(r->response_type, response_type);

    if (http_send_header(fd, r) < 0)
        return -1;
    
    return send(fd, r->response, r->response_len, 0);
}

