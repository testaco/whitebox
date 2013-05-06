#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/wait.h>

#define FLUIDSP_DRIVER_NAME "fluidsp"
#define FLUIDSP_EVENT_MAX_SIZE 496

struct fluidsp_core;
struct fluidsp_event;

struct fluidsp_core_info {
    const char* name;
    const char* pdev_name;
    uint32_t id;

    uint32_t irq;
    uint32_t ctrlreg;

    int (*verify_cmd) (struct fluidsp_core*, unsigned int, void*, size_t);
    int (*patch_event) (struct fluidsp_core*, struct fluidsp_event*);
};
    

struct fluidsp_event {
    //struct list_head list;
    uint32_t size;
    uint16_t msg_id;
    uint16_t type;
    union {
        uint16_t msg16[FLUIDSP_EVENT_MAX_SIZE / 2];
        uint32_t msg32[FLUIDSP_EVENT_MAX_SIZE / 4];
    } data;
};

struct fluidsp_info {
    // Link to all cores
    uint32_t core_count;
    struct fluidsp_core_info* core;

    // max message size

    // Tasks, queues, and images

    // LUT's
};

extern int fluidsp_init_info(struct fluidsp_info* info);

struct fluidsp_core {
    struct mutex lock;
    const char* name;
    unsigned id;

    uint32_t irq;
    uint32_t ctrlreg;

    struct fluidsp_info* info;

    // TODO: stuff relating to RPC's and available operations

    unsigned num_commands;
    unsigned num_events;

    wait_queue_head_t state_wait;
    unsigned state;

    struct platform_device* pdev;
    // struct clk* clk;
    int open_count;

    // pmem regions

    int (*verify_cmd) (struct fluidsp_core*, unsigned int, void*, size_t);
    int (*patch_event) (struct fluidsp_core*, struct fluidsp_event*);
};

extern int fluidsp_publish_cdevs(struct fluidsp_core* cores, unsigned count);
extern void fluidsp_cleanup_cdevs(void);
