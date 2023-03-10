// Morse Code driver:
#include <linux/module.h>
#include <linux/miscdevice.h>		// for misc-driver calls.

// #error Are we building this file?

#define MY_DEVICE_FILE  "morse-code"

static int __init morsecode_init(void)
{
	printk(KERN_INFO "----> My Morse Code driver init().\n");
	return 0;
}

static void __exit morsecode_exit(void)
{
	printk(KERN_INFO "<---- My Morse Code driver exit().\n");
}

// Link our init/exit functions into the kernel's code.
module_init(morsecode_init);
module_exit(morsecode_exit);

// Information about this module:
MODULE_AUTHOR("Travis");
MODULE_DESCRIPTION("Morse Code driver");
MODULE_LICENSE("GPL"); // Important to leave as GPL
