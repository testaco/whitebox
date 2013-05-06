#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "fluidsp.h"

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int fluidsp_debug = 2;

/*
 * Global info settings
 */
static struct fluidsp_info fluidsp_info;

/*
 * Global list of cores
 */
static struct fluidsp_core* fluidsp_cores;

// TODO: Add in the other globals like fluid_sp_info, list of fluidsp_cores

/*
 * User can change verbosity of the driver
 */
module_param(fluidsp_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(fluidsp_debug, "fluiDsp debugging level, >0 is verbose");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (fluidsp_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)


static int fluidsp_probe(struct platform_device* pdev) {
    unsigned count;
    int ret, i;

    d_printk(2, "fluiDsp probe\n");

    ret = fluidsp_init_info(&fluidsp_info);
    if (ret)
        return ret;

    count = fluidsp_info.core_count;

    fluidsp_cores = kzalloc(
        sizeof(struct fluidsp_core) * count,
        GFP_KERNEL);
    if (!fluidsp_cores)
        return -ENOMEM;

    for (i = 0; i < count; ++i) {
        struct fluidsp_core* core = fluidsp_cores + i;
        mutex_init(&core->lock);
        init_waitqueue_head(&core->state_wait);
        core->info = &fluidsp_info;
        core->name = fluidsp_info.core[i].name;
        core->id = fluidsp_info.core[i].id;
        core->verify_cmd = fluidsp_info.core[i].verify_cmd;
        core->patch_event = fluidsp_info.core[i].patch_event;
        core->pdev = platform_device_alloc(fluidsp_info.core[i].pdev_name, -1);
        if (!core->pdev)
            return -ENOMEM;

        ret = platform_device_add(core->pdev);
        if (ret)
            goto fail_register_core;
        d_printk(1, "%s\n", core->name);
    }

    d_printk(1, "%d cores\n", count);

    return fluidsp_publish_cdevs(fluidsp_cores, count);
    return 0;

fail_register_core:
    for (i = 0; i < count; ++i) {
        struct fluidsp_core* core = fluidsp_cores + i;
        platform_device_unregister(core->pdev);
    }
    return ret;
}

static int fluidsp_remove(struct platform_device* pdev) {
    unsigned count;
    int i;

    d_printk(2, "fluiDsp remove\n");

    fluidsp_cleanup_cdevs();

    count = fluidsp_info.core_count;
    for (i = 0; i < count; ++i) {
        struct fluidsp_core* core = fluidsp_cores + i;
        platform_device_unregister(core->pdev);
    }

    kfree(fluidsp_cores);

    return 0;
}

static struct platform_driver fluidsp_driver = {
    .probe = fluidsp_probe,
    .remove = fluidsp_remove,
    .driver = {
        .name = FLUIDSP_DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

static struct platform_device* fluidsp_device;

static int __init fluidsp_init_module(void) {
    int ret;
    d_printk(2, "fluiDsp init\n");
    ret = platform_driver_register(&fluidsp_driver);
    if (ret < 0) {
        d_printk(0, "Couldn't register driver");
        goto Done;
    }

    fluidsp_device = platform_device_alloc("fluidsp", 0);
    if (!fluidsp_device) {
        d_printk(0, "Couldn't allocate device");
        ret = -ENOMEM;
        goto Done;
    }

    ret = platform_device_add(fluidsp_device);
    if (ret < 0) {
        d_printk(0, "Couldn't radd device");
        goto Done;
    }

Done:
    return ret;
}

static void __exit fluidsp_cleanup_module(void) {
    d_printk(2, "fluiDsp cleanup\n");
    platform_device_unregister(fluidsp_device);
    platform_driver_unregister(&fluidsp_driver);
}

module_init(fluidsp_init_module);
module_exit(fluidsp_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Testa, testac@gmail.com");
MODULE_DESCRIPTION("Fluid Signal Processing");
