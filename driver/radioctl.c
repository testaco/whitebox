/*
 * radioctl.c - Radio Control Kernel Module
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

//#include "TOPLEVEL_hw_platform.h"
#define RADIO_0                         0x40050400U


/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int radioctl_debug = 1;

/*
 * User can change verbosity of the driver
 */
module_param(radioctl_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(radioctl_debug, "Radio control driver");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (radioctl_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

/*
 * Device major number
 */
static uint radioctl_major = 166;

/*
 * User can change the major number
 */
module_param(radioctl_major, uint, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(radioctl_major, "Radioctl driver major number");

/*
 * Device name
 */
static char * radioctl_name = "radioctl";

/*
 * Device access lock. Only one process can access the driver at a time
 */
static int radioctl_lock = 0;

/*
 * Device "data"
 */
static char radioctl_str[] = "This is the simplest loadable kernel module\n";
static char *radioctl_end;

/*
 * GPIO pin definitions
 *
static struct gpio radioctl_gpios[] = {
    { 0, GPIOF_OUT_INIT_LOW, "DAC_D[0]" },
    { 1, GPIOF_IN, "VCO_LD" }
};*/

/*
 * Device open
 */
static int radioctl_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	/*
	 * One process at a time
	 */
	if (radioctl_lock ++ > 0) {
		ret = -EBUSY;
		goto Done;
	}

	/*
 	 * Increment the module use counter
 	 */
	try_module_get(THIS_MODULE);

	/*
 	 * Do open-time calculations
 	 */
	radioctl_end = radioctl_str + strlen(radioctl_str);


Done:
	d_printk(2, "lock=%d\n", radioctl_lock);
	return ret;
}

/*
 * Device close
 */
static int radioctl_release(struct inode *inode, struct file *file)
{
	/*
 	 * Release device
 	 */
	radioctl_lock = 0;

	/*
 	 * Decrement module use counter
 	 */
	module_put(THIS_MODULE);

	d_printk(2, "lock=%d\n", radioctl_lock);
	return 0;
}

/* 
 * Device read
 */
static ssize_t radioctl_read(struct file *filp, char *buffer,
			 size_t length, loff_t * offset)
{
	char * addr;
	unsigned int len = 0;
	int ret = 0;

	/*
 	 * Check that the user has supplied a valid buffer
 	 */
	if (! access_ok(0, buffer, length)) {
		ret = -EINVAL;
		goto Done;
	}

	/*
 	 * Get access to the device "data"
 	 */
	addr = radioctl_str + *offset;

	/*
	 * Check for an EOF condition.
	 */
	if (addr >= radioctl_end) {
		ret = 0;
		goto Done;
	}

	/*
 	 * Read in the required or remaining number of bytes into
 	 * the user buffer
 	 */
	len = addr + length < radioctl_end ? length : radioctl_end - addr;
	strncpy(buffer, addr, len);
	*offset += len;
	ret = len;

Done:
	d_printk(3, "length=%d,len=%d,ret=%d\n", length, len, ret);
	return ret;
}

/* 
 * Device write
 */
static ssize_t radioctl_write(struct file *filp, const char *buffer,
			  size_t length, loff_t * offset)
{
	d_printk(3, "length=%d\n", length);
	return -EIO;
}

/*
 * Device operations
 */
static struct file_operations radioctl_fops = {
	.read = radioctl_read,
	.write = radioctl_write,
	.open = radioctl_open,
	.release = radioctl_release
};

static int __init radioctl_init_module(void)
{
	int ret = 0;

	/*
 	 * check that the user has supplied a correct major number
 	 */
	if (radioctl_major == 0) {
		printk(KERN_ALERT "%s: radioctl_major can't be 0\n", __func__);
		ret = -EINVAL;
		goto Done;
	}

	/*
 	 * Register device
 	 */
	ret = register_chrdev(radioctl_major, radioctl_name, &radioctl_fops);
	if (ret < 0) {
		printk(KERN_ALERT "%s: registering device %s with major %d "
				  "failed with %d\n",
		       __func__, radioctl_name, radioctl_major, ret);
		goto Done;
	}

    /*
     * Register gpios
     */
    ret = gpio_request(0, "DAC_EN");
	if (ret < 0) {
		printk(KERN_ALERT "%s: requesting gpio 0 DAC_EN"
				  "failed with %d\n",
		       __func__, ret);
		goto Done;
	}

    /*
     * Set gpio direction
     */
    ret = gpio_direction_input(0);
	if (ret < 0) {
		printk(KERN_ALERT "%s: setting gpio direction 0 DAC_EN"
				  "failed with %d\n",
		       __func__, ret);
		goto Done;
	}

    /*
     * Export gpios
     *
    ret = gpio_export(0, FALSE);
	if (ret < 0) {
		printk(KERN_ALERT "%s: exporing gpio %d"
				  "failed with %d\n",
		       __func__, 0, ret);
		goto Done;
	}*/

	
Done:
	d_printk(1, "name=%s,major=%d\n", radioctl_name, radioctl_major);

	return ret;
}
static void __exit radioctl_cleanup_module(void)
{
    /*
     * Unexport gpios
     *
    gpio_unexport(0);
    */

    /*
     * Unregister gpios
     */
    gpio_free(0);

	/*
	 * Unregister device
	 */
	unregister_chrdev(radioctl_major, radioctl_name);

	d_printk(1, "%s\n", "clean-up successful");
}

module_init(radioctl_init_module);
module_exit(radioctl_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Testa, testac@gmail.com");
MODULE_DESCRIPTION("Radio control driver");
