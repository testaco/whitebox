#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "reactor.h"

struct reactor *reactor_new() {
    struct reactor *reactor;
    reactor = malloc(sizeof(struct reactor));
    reactor->base = event_base_new();
    INIT_LIST_HEAD(&reactor->resources);
    if (!reactor->base) {
        free(reactor);
        return 0;
    }
    return reactor;
}


void reactor_close_all_cb(evutil_socket_t fd, short event, void *arg) {
    printf("reactor close all cb\n");
}

void reactor_close_all(struct reactor *reactor,
        struct instr_result *result) {
    struct io_resource *r;
    struct instr_result *super_result = instr_result_new(reactor,
            reactor_close_all_cb, result);
    printf("closing all\n");
    list_for_each_entry_reverse(r, &reactor->resources, reactor_list) {
        // TODO: this could be a chain of reactions...
        reactor_close_resource(reactor, r, super_result);
    }
}

void reactor_free(struct reactor *reactor) {
    struct io_resource *r, *n;
    //reactor_close_all(reactor);
    list_for_each_entry_safe_reverse(r, n, &reactor->resources, reactor_list) {
        r->ops->free(r);
        list_del(&r->reactor_list);
    }
    event_base_free(reactor->base);
    free(reactor);
}

struct io_resource *reactor_add_resource(struct reactor *reactor,
        char *name, resource_factory factory, void *factory_data) {
    struct io_resource *r = factory(reactor, factory_data);
    if (!r)
        return NULL;
    r->name = name;
    list_add_tail(&r->reactor_list, &reactor->resources);
    return r;
}

void reactor_init_io_resource(struct reactor *reactor,
        struct io_resource *r, struct io_ops *ops) {
    INIT_LIST_HEAD(&r->subscribers);
    r->reactor = reactor;
    r->buffer = evbuffer_new();
    r->ops = ops;
    r->opened = 0;
    r->closing = 0;
}

int reactor_open_resource(struct reactor *reactor,
        struct io_resource *resource) {
    int ret = resource->ops->open(resource);
    if (ret >= 0)
        resource->opened = 1;
    return ret;
}

int reactor_close_resource(struct reactor *reactor,
        struct io_resource *resource,
        struct instr_result *result) {
    int ret = 0;
    if (resource->opened) {
        resource->closing = 1;
        printf("reactor executing close\n");
        ret = resource->ops->close(resource, result);
    }
    resource->opened = 0;
    return ret;
}

void reactor_subscribe(struct reactor *reactor,
        struct io_resource *publisher, struct io_resource *subscriber) {
    list_add(&subscriber->subscription, &publisher->subscribers);
}

int reactor_publish(struct io_resource *resource) {
    struct io_resource *subscriber;
    list_for_each_entry(subscriber, &resource->subscribers, subscription) {
        //printf("publisher: %s subscriber: %s\n", resource->name, subscriber->name);
        subscriber->ops->write(subscriber, resource);
    }
    return 0;
}

int reactor_run(struct reactor *reactor) {
    int ret;
    ret = event_base_dispatch(reactor->base);
    //reactor_close_all(reactor);
    return ret;
}


int io_resource_open(struct io_resource *r) {
    printf("open %s\n", r->name);
    return 0;
}


int io_resource_close(struct io_resource *r, struct instr_result *result) {
    printf("close %s\n", r->name);
    return instr_result_complete(result, 0);
}

ssize_t io_resource_write(struct io_resource *r, struct io_resource *from) {
    //printf("write %s\n", r->name);
    return evbuffer_add_buffer(r->buffer, from->buffer);
}

ssize_t io_resource_write_file(struct io_resource *r, int fd, off_t offset,
        off_t length) {
    //printf("write %s\n", r->name);
    return evbuffer_add_file(r->buffer, fd, offset, length);
}

ssize_t io_resource_write_data(struct io_resource *r, void *data, size_t count) {
    //printf("write %s\n", r->name);
    return evbuffer_add(r->buffer, data, count);
}

void io_resource_free(struct io_resource *r) {
    printf("free %s\n", r->name);
}

void whitebox_sink_cb(evutil_socket_t fd, short event, void *arg) {
    void *dest;
    struct io_resource *r = (struct io_resource *)arg;
    struct whitebox_sink *wbsink = container_of(r,
        struct whitebox_sink, r);
    struct whitebox *wb = &wbsink->wb;
    size_t space_available = ioctl(whitebox_fd(wb), W_MMAP_WRITE, &dest);
    size_t data_available = evbuffer_get_length(r->buffer);
    size_t count = space_available < data_available ?
            space_available : data_available;
    int ret;
    if (r->closing && data_available == 0) {
        printf("closing for good total bytes %ld\n", wbsink->bytes);
        event_del(r->event);
        fsync(whitebox_fd(wb));
        whitebox_munmap(wb);
        whitebox_close(wb);
        r->opened = r->closing = 0;
    } else if (count > 0) {
        count = evbuffer_remove(r->buffer, dest, count);
        if (r->closing)
            printf("w");
        ret = write(whitebox_fd(wb), 0, count);
        if (ret != count)
            printf("problem space=%d data=%d count=%d ret=%d total_written=%ld\n", space_available, data_available, count, ret, wbsink->bytes);
        assert(ret == count);
        wbsink->bytes += count;
    }
}

int whitebox_sink_open(struct io_resource *r) {
    int ret;
    struct whitebox_sink *wbsink = container_of(r, struct whitebox_sink, r);
    struct whitebox *wb = &wbsink->wb;
    wbsink->bytes = 0;
    wbsink->buffering = 1;
    ret = whitebox_open(wb, "/dev/whitebox", O_RDWR | O_NONBLOCK,
            wbsink->config.sample_rate);
    whitebox_tx_set_latency(wb, wbsink->config.latency_ms);
    if (ret < 0)
        return ret;
    ret = whitebox_mmap(wb);
    if (ret < 0)
        return ret;
    // TODO: set the carrier frequency somehow!
    whitebox_tx(wb, 145e6);
    printf("whitebox_open latency=%dms\n", whitebox_tx_get_latency(wb));
    r->event = event_new(r->reactor->base, whitebox_fd(wb),
            EV_WRITE | EV_PERSIST, whitebox_sink_cb, r);
    event_add(r->event, NULL);
    return ret;
}

int whitebox_sink_close(struct io_resource *r, struct instr_result *result) {
    struct whitebox_sink *wbsink = container_of(r, struct whitebox_sink, r);
    struct whitebox *wb = &wbsink->wb;
    int val;
    printf("closing whitebox %d %d\n", r->closing, evbuffer_get_length(r->buffer));
    //fsync(whitebox_fd(wb));
    //event_del(r->event);
    //whitebox_munmap(wb);
    //val = whitebox_close(wb);
    //return instr_result_complete(result, val);
    return 0;
}

void whitebox_sink_free(struct io_resource *r) {
    struct whitebox_sink *wbsink = container_of(r, struct whitebox_sink, r);
    //evbuffer_free(->buffer);
    free(wbsink);
}

ssize_t whitebox_sink_write(struct io_resource *r, struct io_resource *from) {
    //struct whitebox_sink *wbsink = container_of(r, struct whitebox_sink, r);
    //printf("whitebox_sink_write\n");
    return evbuffer_add_buffer(r->buffer, from->buffer);
}

struct io_ops whitebox_sink_ops = {
    .open = whitebox_sink_open,
    .close = whitebox_sink_close,
    //.read = io_resource_read,
    .write = whitebox_sink_write,
    .free = whitebox_sink_free,
};

struct io_resource *whitebox_sink_new(struct reactor *reactor, void *data) {
    struct whitebox_sink *wbsink;
    wbsink = malloc(sizeof(struct whitebox_sink));
    whitebox_init(&wbsink->wb);
    //wbsink->buffer = evbuffer_new();
    memcpy(&wbsink->config, data, sizeof(struct whitebox_sink_config));
    reactor_init_io_resource(reactor, &wbsink->r, &whitebox_sink_ops);
    return &wbsink->r;
}

void sampler_source_cb(evutil_socket_t fd, short event, void *arg) {
    struct io_resource *r = (struct io_resource *)arg;
    struct sampler_source *sampler = container_of(r, struct sampler_source, r);
    // publish for io_resource
    io_resource_write_data(r, 0, sizeof(int32_t));
    reactor_publish(r);
    evtimer_add(r->event, &sampler->interval);
}

int sampler_source_open(struct io_resource *r) {
    struct timeval tv = { 0, 0 };
    r->event = evtimer_new(r->reactor->base, sampler_source_cb, r);
    evtimer_add(r->event, &tv);
    return 0;
}

int sampler_source_close(struct io_resource *r, struct instr_result *result) {
    evtimer_del(r->event);
    return instr_result_complete(result, 0);
}

struct io_ops sampler_source_ops = {
    .open = sampler_source_open,
    .close = sampler_source_close,
    //.read = io_resource_read,
    .write = io_resource_write,
    .free = io_resource_free,
};

struct io_resource *sampler_source_new(struct reactor *reactor, void *data) {
    struct sampler_source *sampler_source;
    int sampler_interval_ms = (int)data;
    sampler_source = malloc(sizeof(struct sampler_source));
    reactor_init_io_resource(reactor, &sampler_source->r, &sampler_source_ops);
    sampler_source->interval.tv_sec = 0;
    // Imperically discovered that subtracting 1 jiffy, 10ms makes this just work.
    sampler_source->interval.tv_usec = (long)((sampler_interval_ms-10) * 1000);
    return &sampler_source->r;
}

void file_source_cb(evutil_socket_t fd, short event, void *arg) {
    struct io_resource *r = (struct io_resource *)arg;
    struct timeval tv = { 0, 0 };
    struct file_source *file_source = container_of(r, struct file_source, r);
    struct evbuffer_iovec v[2];
    int size = 1024;
    size_t len;
    int n;

    n = evbuffer_read(r->buffer, fd, size);
    //printf("wrote %d %d\n", n, evbuffer_get_length(r->buffer));

    // publish for io_resource
    //n = evbuffer_reserve_space(r->buffer, size, v, 2);
    //if (n <= 0) return;
    //len = v[0].iov_len;
    //if (len > size)
    //    len = size;
    //v[0].iov_len = read(file_source->fd, v[0].iov_base, size);
    //printf("writing some file %d\n", v[0].iov_len);
    //if (v[0].iov_len <= 0) {
    //    //lseek(file_source->fd, 0, SEEK_SET);
    //    //return;
    //}
    //v[1].iov_len = 0;
    //if (evbuffer_commit_space(r->buffer, v, 2) < 0) {
    //    return;
    //}
    //evtimer_add(r->event, &sampler->interval);
    if (n > 0) {
        reactor_publish(r);
        event_add(r->event, &tv);
    } else {
        //file_source_close(r);
        printf("Closing file\n");
        reactor_close_resource(r->reactor, r, file_source->instr_result);
        //file_source->instr_result->val = n;
    }
}

int file_source_open(struct io_resource *r) {
    struct file_source *file_source = container_of(r, struct file_source, r);
    struct timeval tv = { 0, 0 };
    printf("file_source_open %s\n", file_source->filename);
    file_source->fd = open(file_source->filename, O_RDONLY);
    if (file_source->fd) {
        r->event = event_new(r->reactor->base, file_source->fd,
                EV_READ | EV_PERSIST, file_source_cb, r);
        event_add(r->event, &tv);
    }
    return file_source->fd;
}

int file_source_close(struct io_resource *r, struct instr_result *result) {
    struct file_source *file_source = container_of(r, struct file_source, r);
    struct timeval tv = { 0, 0 };
    // TODO: where the magic will happen..
    printf("file_source_close %d\n", evbuffer_get_length(r->buffer));
    close(file_source->fd);
    event_del(r->event);
    evtimer_add(result->event, &tv);
    return instr_result_complete(result, 0);
}

struct io_ops file_source_ops = {
    .open = file_source_open,
    .close = file_source_close,
    //.read = io_resource_read,
    .write = io_resource_write,
    .free = io_resource_free,
};

struct io_resource *file_source_new(struct reactor *reactor, void *data) {
    struct file_source *file_source;
    struct actor_instr *instr = (struct actor_instr *)data;
    file_source = malloc(sizeof(struct file_source));
    file_source->instr_result = &instr->result;
    file_source->filename = instr->c;
    printf("filename is '%s'", file_source->filename);
    file_source->fd = -1;
    reactor_init_io_resource(reactor, &file_source->r, &file_source_ops);
    return &file_source->r;
}

ssize_t upsampler_write(struct io_resource *r, struct io_resource *from) {
    int32_t sample;
    struct upsampler *upsampler = container_of(r, struct upsampler, r);
    size_t size = sizeof(int32_t) * upsampler->rate;
    int n, i, j;
    struct evbuffer_iovec v[2];

    while (evbuffer_remove(from->buffer, &sample, sizeof(int32_t)) > 0 && size > 0) {
        //printf("upsampler_write %d\n", size);
        n = evbuffer_reserve_space(r->buffer, size, v, 2);
        if (n <= 0) return n;
        for (i = 0; i < n && size > 0; ++i) {
            size_t len = v[i].iov_len;
            if (len > size)
                len = size;
            for (j = 0; j < len>>2; j++) {
                ((int32_t*)v[i].iov_base)[j] = sample;
            }
            v[i].iov_len = len;
            size -= len;
        }
    }
    if (evbuffer_commit_space(r->buffer, v, i) < 0)
        return -1;

    return reactor_publish(r);
}

struct io_ops upsampler_ops = {
    .open = io_resource_open,
    .close = io_resource_close,
    //.read = io_resource_read,
    .write = upsampler_write,
    .free = io_resource_free,
};

struct io_resource *upsampler_new(struct reactor *reactor, void *data) {
    struct upsampler *upsampler;
    upsampler = malloc(sizeof(struct upsampler));
    upsampler->rate = (int)data;
    reactor_init_io_resource(reactor, &upsampler->r, &upsampler_ops);
    return &upsampler->r;
}

int buffer_sink_open(struct io_resource *r) {
    return 0;
}

int buffer_sink_close(struct io_resource *r, struct instr_result *result) {
    return instr_result_complete(result, 0);
}

ssize_t buffer_sink_write(struct io_resource *r, struct io_resource *from) {
    struct timespec ts;
    struct buffer_sink *buffer_sink = container_of(r, struct buffer_sink, r);
    buffer_sink->count++;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return evbuffer_add(r->buffer, &ts, sizeof(struct timespec));
}

void buffer_sink_free(struct io_resource *r) {
    struct buffer_sink *buffer_sink = container_of(r, struct buffer_sink, r);
    //evbuffer_free(buffer_sink->buffer);
    free(buffer_sink);
}

struct io_ops buffer_sink_ops = {
    .open = buffer_sink_open,
    .close = buffer_sink_close,
    //.read = io_resource_read,
    .write = buffer_sink_write,
    .free = buffer_sink_free,
};

struct io_resource *buffer_sink_new(struct reactor *reactor, void *data) {
    struct buffer_sink *buffer_sink;
    buffer_sink = malloc(sizeof(struct buffer_sink));
    //buffer_sink->buffer = evbuffer_new();
    buffer_sink->count = 0;
    reactor_init_io_resource(reactor, &buffer_sink->r, &buffer_sink_ops);
    return &buffer_sink->r;
}

void control_panel_cb(evutil_socket_t fd, short event, void *arg) {
    struct control_panel *control_panel = container_of(arg,
        struct control_panel, r);
    if (control_panel->buttons & KEY_PTT) {
        printf("tx_on\n");
    } else {
        printf("tx_off\n");
    }
}

int control_panel_open(struct io_resource *r) {
    r->event = evtimer_new(r->reactor->base, control_panel_cb, r);
    return 0;
}

ssize_t control_panel_write(struct io_resource *r, struct io_resource *from) {
    struct timeval tv = { 0, 0 };
    struct control_panel *panel = container_of(r, struct control_panel, r);
    panel->buttons = 0;
    evtimer_add(r->event, &tv);
}

struct io_ops control_panel_ops = {
    .open = control_panel_open,
    .close = io_resource_close,
    //.read = io_resource_read,
    .write = control_panel_write,
    .free = io_resource_free,
};

struct io_resource *control_panel_new(struct reactor *reactor, void *data) {
    struct control_panel *control_panel;
    control_panel = malloc(sizeof(struct control_panel));
    reactor_init_io_resource(reactor, &control_panel->r, &control_panel_ops);
    control_panel->buttons = 0;
    control_panel->leds = 0;
    return &control_panel->r;
}

void actor_write_file(struct io_resource *actor, struct io_resource *dest,
        struct actor_instr *instr) {
    char *filename = instr->c;
    struct io_resource *file_source;
    printf("actor write file %s\n", filename);
    file_source = reactor_add_resource(actor->reactor, "file_source",
        file_source_new, instr);
    reactor_subscribe(actor->reactor, file_source, dest);
    if (reactor_open_resource(actor->reactor, file_source) < 0) {
        // TODO: cleanup memory
        struct timeval tv = { 0, 0 };
        instr->result.val = -1;
        evtimer_add(instr->result.event, &tv);
        perror("couldn't open file");
    }
}

void actor_cb(evutil_socket_t fd, short event, void *arg) {
    struct io_resource *actor_resource = (struct io_resource *)arg;
    struct actor *actor = container_of(actor_resource,
        struct actor, r);
    struct actor_instr *instr = &actor->script[actor->pc++];
    struct reactor *reactor = actor_resource->reactor;
    struct timeval tv = { 0, 0 };
    //printf("%s %d %d %s\n", instr->type, instr->a, instr->b, instr->c);

    instr->result.event = actor->r.event;

    if (!instr->type) {
        return;
    } else if (strncmp(instr->type, "halt", 10) == 0) {
        printf("halting\n");
        reactor_close_all(reactor, &instr->result);
        return;
    } else if (strncmp(instr->type, "wait", 10) == 0) {
        printf("waiting %d.%d\n", instr->a, instr->b);
        tv.tv_sec = instr->a;
        tv.tv_usec = instr->b;
    } else if (strncmp(instr->type, "print", 10) == 0) {
        printf("%s\n", instr->c);
    } else if (strncmp(instr->type, "open", 10) == 0) {
        struct io_resource *r = instr->resource;
        reactor_open_resource(reactor, r);
    } else if (strncmp(instr->type, "close", 10) == 0) {
        struct io_resource *r = instr->resource;
        reactor_close_resource(reactor, r, &instr->result);
        return;
    } else if (strncmp(instr->type, "sub", 10) == 0) {
        struct io_resource *publisher = instr->resource;
        struct io_resource *subscriber = (struct io_resource *)instr->c;
        reactor_subscribe(reactor, publisher, subscriber);
    } else if (strncmp(instr->type, "read", 10) == 0) {
        struct io_resource *r = instr->resource;
        //r->ops->read(r, instr->c, instr->a);
    } else if (strncmp(instr->type, "write", 10) == 0) {
        struct io_resource *r = instr->resource;
        io_resource_write_data(r, instr->c, instr->a);
    } else if (strncmp(instr->type, "writef", 10) == 0) {
        struct io_resource *r = instr->resource;
        actor_write_file(&actor->r, r, instr);
        return;
    } else {
        printf("ERROR UNKNOWN INSTRUCTION TYPE %s\n", instr->type);
        return;
    }

    evtimer_add(actor->r.event, &tv);
}

int actor_open(struct io_resource *r) {
    struct timeval tv = { 0, 0 };
    struct actor *actor = container_of(r, struct actor, r);
    actor->pc = 0;
    r->event = evtimer_new(r->reactor->base, actor_cb, r);
    evtimer_add(r->event, &tv);
    printf("open %s\n", r->name);
    return 0;
}

int actor_close(struct io_resource *r, struct instr_result *result) {
    //struct actor *actor = container_of(r, struct actor, r);
    event_free(r->event);
    return instr_result_complete(result, 0);
}

struct io_ops actor_ops = {
    .open = actor_open,
    .close = actor_close,
    //.read = io_resource_read,
    .write = io_resource_write,
    .free = io_resource_free,
};

struct io_resource *actor_new(struct reactor *reactor, void *data) {
    struct actor *actor;
    actor = malloc(sizeof(struct actor));
    actor->pc = 0;
    reactor_init_io_resource(reactor, &actor->r, &actor_ops);
    return &actor->r;
}

void actor_add_instr(struct actor *actor, char *type, int a, int b, char *c) {
    struct actor_instr *instr = &actor->script[actor->pc++];
    instr->type = type;
    instr->resource = NULL;
    instr->a = a;
    instr->b = b;
    instr->c = c;
}

void actor_add_io_instr(struct actor *actor, char *type,
        struct io_resource *r, void *data, int count) {
    struct actor_instr *instr = &actor->script[actor->pc++];
    instr->type = type;
    instr->resource = r;
    instr->a = count;
    instr->b = 0;
    instr->c = data;
}

void actor_add_open_instr(struct actor *actor, struct io_resource *r) {
    struct actor_instr *instr = &actor->script[actor->pc++];
    instr->type = "open";
    instr->resource = r;
    instr->a = 0;
    instr->b = 0;
    instr->c = 0;
}

void actor_add_close_instr(struct actor *actor,
        struct io_resource *r) {
    struct actor_instr *instr = &actor->script[actor->pc++];
    instr->type = "close";
    instr->resource = r;
    instr->a = 0;
    instr->b = 0;
    instr->c = 0;
}

void actor_add_sub_instr(struct actor *actor,
        struct io_resource *publisher,
        struct io_resource *subscriber) {
    struct actor_instr *instr = &actor->script[actor->pc++];
    instr->type = "sub";
    instr->resource = publisher;
    instr->a = 0;
    instr->b = 0;
    instr->c = subscriber;
}

struct instr_result *instr_result_new(struct reactor *reactor,
        void (*cb)(evutil_socket_t fd, short event, void *arg), void *data) {
    struct instr_result *result = malloc(sizeof(struct instr_result));
    result->event = evtimer_new(reactor->base, cb, data);
    return result;
}

int instr_result_complete(struct instr_result *result, int val) {
    struct timeval tv = { 0, 0 };
    if (result) {
        result->val = val;
        evtimer_add(result->event, &tv);
    }
    return val;
}
