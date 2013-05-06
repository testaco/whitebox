#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "fluidsp.h"

struct fluidsp_device {
    struct fluidsp_core* core;

    const char* name;
    struct device* device;
    struct cdev cdev;
};

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int fluidsp_driver_debug = 1;

/*
 * User can change verbosity of the driver
 */
module_param(fluidsp_driver_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(fluidsp_driver_debug, "fluiDsp driver debugging level, >0 is verbose");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (fluidsp_driver_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

static unsigned fluidsp_device_count;
static struct fluidsp_device* fluidsp_devices;

static struct fluidsp_device* inode_to_device(struct inode* inode) {
    unsigned n = MINOR(inode->i_rdev);
    if (n < fluidsp_device_count) {
        if (fluidsp_devices[n].device)
            return fluidsp_devices + n;
    }
    return NULL;
}

static int fluidsp_open(struct inode* inode, struct file* filp) {
    struct fluidsp_device* adev;
    int ret;

    ret = nonseekable_open(inode, filp);
    if (ret < 0)
        return ret;

    adev = inode_to_device(inode);
    if (!adev)
        return -ENODEV;

    d_printk(1, "Opening %s", adev->name);

    // TODO: fluidsp_get

    filp->private_data = adev;

    return 0;
}

static dev_t fluidsp_devno;
static struct class* fluidsp_class;

static struct file_operations fluidsp_fops = {
    .owner = THIS_MODULE,
    .open = fluidsp_open,
    //.unlocked_ioctl = fluidsp_ioctl,
    //.release = fluidsp_release,
};

static int fluidsp_create(struct fluidsp_device* adev, const char* name,
                          struct device* parent, dev_t devt) {
    struct device* dev;
    int ret;

    d_printk(1, "%s %s %d %d\n", dev_name(parent), name, MAJOR(devt), MINOR(devt));
    
    dev = device_create(fluidsp_class, parent, devt, "%s", name);
    if (IS_ERR(dev))
        goto fail_create_device;

    cdev_init(&adev->cdev, &fluidsp_fops);
    adev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&adev->cdev, devt, 1);
    if (ret < 0) {
        goto fail_create_cdev;
    } else {
        adev->device = dev;
        adev->name = name;
    }
    return 0;

fail_create_cdev:
    d_printk(0, "Failed to create cdev %s", name);
    cdev_del(&adev->cdev);
fail_create_device:
    d_printk(0, "Failed to create device %s", name);
    device_destroy(fluidsp_class, devt);
    return -EINVAL; // TODO: what error return?
}

static void fluidsp_destroy(struct fluidsp_device* adev) {
    if (adev) {
        d_printk(1, "%s\n", adev->name);
        cdev_del(&adev->cdev);
        device_destroy(fluidsp_class, adev->device->devt);
    }
}

int fluidsp_publish_cdevs(struct fluidsp_core* cores, unsigned count) {
    int ret, i;

    fluidsp_devices = kzalloc(sizeof(struct fluidsp_device) * count, GFP_KERNEL);
    if (!fluidsp_devices) {
        d_printk(0, "Couldn't alloc fluid devices\n");
        return -ENOMEM;
    }

    fluidsp_class = class_create(THIS_MODULE, "fluidsp");
    if (IS_ERR(fluidsp_class))
        goto fail_create_class;

    ret = alloc_chrdev_region(&fluidsp_devno, 0, count, "fluidsp");
    if (ret < 0)
        goto fail_alloc_region;

    fluidsp_device_count = count;
    for (i = 0; i < fluidsp_device_count; ++i) {
        ret = fluidsp_create(fluidsp_devices + i,
            cores[i].name, &cores[i].pdev->dev,
            MKDEV(MAJOR(fluidsp_devno), i));
        if (ret < 0)
            goto fail_create_devices;
    }

    return ret;

fail_create_devices:
    for (i = 0; i < fluidsp_device_count; ++i) {
        if (fluidsp_devices + i)
            fluidsp_destroy(fluidsp_devices + i);
    }
fail_alloc_region:
    class_destroy(fluidsp_class);
fail_create_class:
    kfree(fluidsp_devices);
    d_printk(0, "Failed to create the fluid class\n");
    return -EINVAL;
}

void fluidsp_cleanup_cdevs() {
    int i;
    for (i = 0; i < fluidsp_device_count; ++i) {
        if (fluidsp_devices + i)
            fluidsp_destroy(fluidsp_devices + i);
    }

    class_destroy(fluidsp_class);

    kfree(fluidsp_devices);
}
