#ifndef __WHITEBOX_REACTOR_H
#define __WHITEBOX_REACTOR_H

#include <stdlib.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "whitebox.h"
#include "list.h"

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})


#define KEY_PTT (1 << 0)
#define KEY_DIT (1 << 1)
#define KEY_DAH (1 << 2)

#define LED_0   (1 << 0)
#define LED_1   (1 << 1)
#define LED_2   (1 << 2)
#define LED_3   (1 << 3)
#define LED_4   (1 << 4)

struct io_resource;
struct instr_result;

struct reactor {
    struct event_base *base;
    struct list_head resources;
};

typedef struct io_resource * (*resource_factory)(struct reactor *, void *);

struct reactor *reactor_new();
void reactor_free(struct reactor *reactor);
int reactor_run(struct reactor *reactor);
struct io_resource *reactor_add_resource(struct reactor *reactor, char *name, resource_factory factory, void *factory_data);

struct io_ops {
    int (*open)(struct io_resource *r);
    int (*close)(struct io_resource *r, struct instr_result *result);
    //ssize_t (*read)(struct io_resource *r, void *buf, size_t count);
    ssize_t (*write)(struct io_resource *r, struct io_resource *from);
    void (*free)(struct io_resource *r);
};

struct io_resource {
    struct reactor *reactor;
    char *name;
    struct event *event;
    struct evbuffer *buffer;
    struct io_ops *ops;

    struct list_head reactor_list;
    struct list_head subscribers;
    struct list_head subscription;

    int opened;
    int closing;
};

int reactor_open_resource(struct reactor *reactor,
        struct io_resource *resource);

int reactor_close_resource(struct reactor *reactor,
        struct io_resource *resource,
        struct instr_result *result);

void reactor_subscribe(struct reactor *reactor,
        struct io_resource *publisher, struct io_resource *subscriber);

int reactor_publish(struct io_resource *resource);

struct sampler_source {
    struct io_resource r;
    struct timeval interval;
};

struct io_resource *sampler_source_new(struct reactor *reactor, void *data);

struct upsampler {
    struct io_resource r;
    int rate;
};

struct file_source {
    struct io_resource r;
    struct instr_result *instr_result;
    char *filename;
    int fd;
    int quantum;
};

struct io_resource *file_source_new(struct reactor *reactor, void *data);

struct io_resource *upsampler_new(struct reactor *reactor, void *data);

struct buffer_sink {
    struct io_resource r;
    int count;
};

struct io_resource *buffer_sink_new(struct reactor *reactor, void *data);

struct whitebox_sink_config {
    int sample_rate;
    int latency_ms;
};

struct whitebox_sink {
    struct io_resource r;
    struct whitebox wb;
    struct whitebox_sink_config config;
    int buffering;
    long bytes;
};

struct io_resource *whitebox_sink_new(struct reactor *reactor, void *data);

struct control_panel {
    struct io_resource r;
    short buttons;
    short leds;
    short vol;
};

struct io_resource *control_panel_new(struct reactor *reactor, void *data);

struct instr_result {
    int val;
    struct event *event;
};

struct instr_result *instr_result_new(struct reactor *reactor, void (*cb)(evutil_socket_t fd, short event, void *arg), void *data);
int instr_result_complete(struct instr_result *result, int val);

struct actor_instr {
    char *type;
    struct io_resource *resource;
    int a;
    int b;
    void *c;
    struct instr_result result;
};

struct actor {
    struct io_resource r;
    int pc;
    struct actor_instr script[255];
};

struct io_resource *actor_new(struct reactor *reactor, void *data);

void actor_add_instr(struct actor *actor, char *type, int a, int b, char *c);
void actor_add_io_instr(struct actor *actor, char *type,
        struct io_resource *r, void *data, int count);
void actor_add_open_instr(struct actor *actor,
        struct io_resource *r);
void actor_add_close_instr(struct actor *actor,
        struct io_resource *r);
void actor_add_sub_instr(struct actor *actor,
        struct io_resource *publisher,
        struct io_resource *subscriber);

#endif /* __WHITEBOX_REACTOR_H */
