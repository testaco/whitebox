#include "fluidsp.h"

#include <linux/module.h>

// TODO: This should use the device tree!

#define FLUIDSP_CORE_SIGNAL_GENERATOR 0
#define FLUIDSP_CORE_EXCITER          1

#define FLUIDSP_CORE(n, irq_loc, ctrlreg_loc, verify_cmd_func, patch_event_func) \
    { .name = #n, .pdev_name = "fluidsp_" #n, .id = FLUIDSP_CORE_##n, \
    .irq = irq_loc, .ctrlreg = ctrlreg_loc, \
    .verify_cmd = verify_cmd_func, .patch_event = patch_event_func }

static struct fluidsp_core_info core_info[] = {
    FLUIDSP_CORE(SIGNAL_GENERATOR, 0x01, 0x4001, NULL, NULL),
    FLUIDSP_CORE(EXCITER, 0x02, 0x4002, NULL, NULL),
};

int fluidsp_init_info(struct fluidsp_info* info) {
    info->core_count = ARRAY_SIZE(core_info);
    info->core = core_info;

    return 0;
}
