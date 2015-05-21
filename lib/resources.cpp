#include <cstring>
#include "resources.h"

source_next source;
sink_cb sink;

int resource_init(struct resource *r) {
    r->data = r->ops->init();
    if (r->data != 0)
        return 0;
    return -1;
}

void resource_close(struct resource *r) {
    r->ops->close(r->data);
}

int resource_descriptors_count(struct resource *r) {
    return r->ops->descriptors_count(r->data);
}

int resource_descriptors(struct resource *r,
        struct pollfd *ufds, int count) {
    return r->ops->descriptors(r->data, ufds, count);
}

void resource_handler(struct resource *r,
        struct pollfd *ufds, int count) {
    r->ops->handler(r->data, ufds, count);
}

int resource_setup(struct resource *r, const char *name, struct resource_ops *ops) {
    strncpy(r->name, name, 255);
    r->ops = ops;
    return resource_init(r);
}

