// Morse Code driver:
#include <linux/module.h>
#include <linux/miscdevice.h>		// for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

// #error Are we building this file?

#define MY_DEVICE_FILE  "morse-code"

/******************************************************
 * Callbacks
 ******************************************************/
static ssize_t my_read(struct file *file,
		char *buff, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "demo_miscdrv::my_read(), buff size %d, f_pos %d\n",
			(int) count, (int) *ppos);

	return 0;  // # bytes actually read.
}

static ssize_t my_write(struct file *file,
		const char *buff, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "demo_miscdrv: In my_write(): ");


	// Return # bytes actually written.
	return 0;
}

/******************************************************
 * Misc support
 ******************************************************/
// Callbacks:  (structure defined in /linux/fs.h)
static struct file_operations my_fops = {
	.owner    =  THIS_MODULE,
	.read     =  my_read,
	.write    =  my_write,
};

// Character Device info for the Kernel:
static struct miscdevice my_miscdevice = {
		.minor    = MISC_DYNAMIC_MINOR,         // Let the system assign one.
		.name     = MY_DEVICE_FILE,             // /dev/.... file.
		.fops     = &my_fops                    // Callback functions.
};

/******************************************************
 * Driver initialization and exit:
 ******************************************************/
static int __init morsecode_init(void)
{
	int ret;

	printk(KERN_INFO "----> My Morse Code driver init().\n");

	// Register as a misc driver:
	ret = misc_register(&my_miscdevice);

	return ret;
}

static void __exit morsecode_exit(void)
{
	printk(KERN_INFO "<---- My Morse Code driver exit().\n");

	// Unregister misc driver
	misc_deregister(&my_miscdevice);
}

// Link our init/exit functions into the kernel's code.
module_init(morsecode_init);
module_exit(morsecode_exit);

// Information about this module:
MODULE_AUTHOR("Travis");
MODULE_DESCRIPTION("Morse Code driver");
MODULE_LICENSE("GPL"); // Important to leave as GPL
