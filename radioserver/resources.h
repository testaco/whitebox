#ifndef __RESOURCES_H__
#define __RESOURCES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pollfd;

struct resource_ops {
    // Create and initialize the resource in memory.
    void *(*init)();

    // Close and free the resource.
    void (*close)(void *);

    // How many descriptors this resource needs poll to track.
    int (*descriptors_count)(void *);

    // Fill in the poll descriptor list.
    int (*descriptors)(void *, struct pollfd *, int);

    // Called every main loop iteration to notify the resource of poll updates.
    void (*handler)(void *, struct pollfd *, int);
};

// The object tracked by the main loop.
struct resource {
    char name[256];
    struct resource_ops *ops;
    void *data;

    int descriptor_offset;
    int descriptor_count;
};

int resource_init(struct resource *);
void resource_close(struct resource *);
int resource_descriptors_count(struct resource *);
int resource_descriptors(struct resource *,
        struct pollfd *, int);
void resource_handler(struct resource *,
        struct pollfd *, int);
int resource_setup(struct resource *, const char *, struct resource_ops *);

typedef int16_t (*source_next)();
extern source_next source;

typedef void (*sink_cb)(int16_t sample);
extern sink_cb sink;

#ifdef __cplusplus
}
#endif

#endif /* __RESOURCES_H__ */
