// Morse Code driver:
#include <linux/module.h>
#include <linux/miscdevice.h>		// for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/leds.h> 			// led support
#include <linux/kfifo.h>            // FIFO support
#include <linux/ctype.h>

// #error Are we building this file?

// Morse Code Encodings (from http://en.wikipedia.org/wiki/Morse_code)
//   Encoding created by Brian Fraser. Released under GPL.
//
// Encoding description:
// - msb to be output first, followed by 2nd msb... (left to right)
// - each bit gets one "dot" time.
// - "dashes" are encoded here as being 3 times as long as "dots". Therefore
//   a single dash will be the bits: 111.
// - ignore trailing 0's (once last 1 output, rest of 0's ignored).
// - Space between dashes and dots is one dot time, so is therefore encoded
//   as a 0 bit between two 1 bits.
//
// Example:
//   R = dot   dash   dot       -- Morse code
//     =  1  0 111  0  1        -- 1=LED on, 0=LED off
//     =  1011 101              -- Written together in groups of 4 bits.
//     =  1011 1010 0000 0000   -- Pad with 0's on right to make 16 bits long.
//     =  B    A    0    0      -- Convert to hex digits
//     = 0xBA00                 -- Full hex value (see value in table below)
//
// Between characters, must have 3-dot times (total) of off (0's) (not encoded here)
// Between words, must have 7-dot times (total) of off (0's) (not encoded here).
//
static unsigned short morsecode_codes[] = {
		0xB800,	// A 1011 1
		0xEA80,	// B 1110 1010 1
		0xEBA0,	// C 1110 1011 101
		0xEA00,	// D 1110 101
		0x8000,	// E 1
		0xAE80,	// F 1010 1110 1
		0xEE80,	// G 1110 1110 1
		0xAA00,	// H 1010 101
		0xA000,	// I 101
		0xBBB8,	// J 1011 1011 1011 1
		0xEB80,	// K 1110 1011 1
		0xBA80,	// L 1011 1010 1
		0xEE00,	// M 1110 111
		0xE800,	// N 1110 1
		0xEEE0,	// O 1110 1110 111
		0xBBA0,	// P 1011 1011 101
		0xEEB8,	// Q 1110 1110 1011 1
		0xBA00,	// R 1011 101
		0xA800,	// S 1010 1
		0xE000,	// T 111
		0xAE00,	// U 1010 111
		0xAB80,	// V 1010 1011 1
		0xBB80,	// W 1011 1011 1
		0xEAE0,	// X 1110 1010 111
		0xEBB8,	// Y 1110 1011 1011 1
		0xEEA0	// Z 1110 1110 101
};

#define MY_DEVICE_FILE  "morse-code"

/**************************************************************
 * FIFO Support
 *************************************************************/
// Info on the interface:
//    https://www.kernel.org/doc/htmldocs/kernel-api/kfifo.html#idp10765104
// Good example:
//    http://lxr.free-electrons.com/source/samples/kfifo/bytestream-example.c
#define FIFO_SIZE 256	// Must be a power of 2.
static DECLARE_KFIFO(morsecode_fifo, char, FIFO_SIZE);

/******************************************************
 * LED
 ******************************************************/

DEFINE_LED_TRIGGER(ledtrig_morsecode);

#define LED_DOT_TIME_ms 200
#define LED_DASH_TIME_ms (LED_DOT_TIME_ms*3)

#define LED_INTER_CHAR_TIME_ms LED_DOT_TIME_ms
#define LED_CHAR_BREAK_ms (LED_DOT_TIME_ms*3)
#define LED_WORD_BREAK_ms (LED_DOT_TIME_ms*7)

static void led_register(void)
{
	// Setup the trigger's name:
	led_trigger_register_simple("morse-code", &ledtrig_morsecode);
}

static void led_unregister(void)
{
	// Cleanup
	led_trigger_unregister_simple(ledtrig_morsecode);
}

/******************************************************
 * Helper Functions
 ******************************************************/

static int morsecode_dot(void)
{
	printk(KERN_INFO "morse-code: dot\n");

	led_trigger_event(ledtrig_morsecode, LED_FULL);
	msleep(LED_DOT_TIME_ms);
	led_trigger_event(ledtrig_morsecode, LED_OFF);

	if (!kfifo_put(&morsecode_fifo, '.')) {
		// Fifo full
		return -EFAULT;
	}

	return 0;
}

static int morsecode_dash(void)
{
	printk(KERN_INFO "morse-code: dash\n");

	led_trigger_event(ledtrig_morsecode, LED_FULL);
	msleep(LED_DASH_TIME_ms);
	led_trigger_event(ledtrig_morsecode, LED_OFF);

	if (!kfifo_put(&morsecode_fifo, '-')) {
		// Fifo full
		return -EFAULT;
	}

	return 0;
}

static void morsecode_inter_char_break(void)
{
	printk(KERN_INFO "morse-code: break - inter char\n");

	msleep(LED_INTER_CHAR_TIME_ms);
}

static int morsecode_char_break(void)
{
	printk(KERN_INFO "morse-code: break - char\n");

	msleep(LED_CHAR_BREAK_ms);

	if (!kfifo_put(&morsecode_fifo, ' ')) {
		// Fifo full
		return -EFAULT;
	}

	return 0;
}

static int morsecode_word_break(void)
{
	int i = 0;
	printk(KERN_INFO "morse-code: break - word\n");

	msleep(LED_WORD_BREAK_ms);

	for (i = 0; i < 3; i++) {
		if (!kfifo_put(&morsecode_fifo, ' ')) {
			// Fifo full
			return -EFAULT;
		}
	}

	return 0;
}

// Convert ch to morse: flash led + add to kfifo
static int ch_to_morse(char ch)
{
	char lower_ch = tolower(ch);
	int ch_int = lower_ch - 'a';
	unsigned short morse_code = morsecode_codes[ch_int];

	int i = 0;
	int code_size = sizeof(*morsecode_codes) * 8;

	int one_count = 0;        // Number of consecutive ones in code
	bool led_blinked = false; // True when after each "one"

	printk(KERN_INFO "morse-code: lower_ch: %c, ch_int: %d", lower_ch, ch_int);

	for (i=code_size-1; i >= 0; i--) {
		bool code = (morse_code & (1 << i)) != 0;
		if (code) {
			one_count++;
		} else {
			// Exit when consecutive 0's
			if (one_count == 0) {
				break;
			} 
			
			// Breaks in single char
			if (led_blinked) {
				morsecode_inter_char_break();
			}

			if (one_count == 1) {
				if (morsecode_dot()) {
					return -EFAULT;
				}
			} else {
				if (morsecode_dash()) {
					return -EFAULT;
				}
			}
			led_blinked = true;
			one_count = 0;
		}
	}
	return 0;
}

/******************************************************
 * Read / Write
 ******************************************************/

static ssize_t my_read(struct file *file,
		char *buff, size_t count, loff_t *ppos)
{
	int num_bytes_read = 0;

	printk(KERN_INFO "morse-code: Reading buff size %d, f_pos %d\n",
			(int) count, (int) *ppos);
	
	if (kfifo_is_empty(&morsecode_fifo)) {
		return 0;
	}

	if (kfifo_to_user(&morsecode_fifo, buff, count, &num_bytes_read)) {
		return -EFAULT;
	}
	*ppos += num_bytes_read;
	return num_bytes_read;
}

static ssize_t my_write(struct file *file,
		const char *buff, size_t count, loff_t *ppos)
{
	int buff_idx = 0;

	bool word_break = false;  // True after whitespace (not front)
	bool char_break = false;  // True after a char
	bool front_space = true;  // True when front whitespace

	printk(KERN_INFO "morse-code: Converting %d sized string to morse code.\n", count);

	for (buff_idx = 0; buff_idx < count; buff_idx++) {
		char ch;
		// Get the character
		if (copy_from_user(&ch, &buff[buff_idx], sizeof(ch))) {
			return -EFAULT;
		}

		// Set word break to true for whitespace
		if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
			if (!front_space) {
				word_break = true;
				continue;
			}
		}

		// Skip special characters:
		if (!isalpha(ch)) {
			continue;
		}
		
		// Break either word or char
		if (word_break) {
			if (morsecode_word_break()) {
				return -EFAULT;
			}
			
		} else if (char_break) {
			if (morsecode_char_break()){
				return -EFAULT;
			}
		}

		front_space = false;
		word_break = false;
		char_break = true;

		if (ch_to_morse(ch)) {
			return -EFAULT;
		}
	}

	if (!kfifo_put(&morsecode_fifo, '\n')) {
		// Fifo full
		return -EFAULT;
	}

	// Return # bytes actually written.
	*ppos += count;
	return count;
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
	printk(KERN_INFO "----> morse-code: init() file /dev/%s.\n", MY_DEVICE_FILE);

	// Register as a misc driver:
	ret = misc_register(&my_miscdevice);

	// KFIFO
	INIT_KFIFO(morsecode_fifo);

	// LED:
	led_register();

	return ret;
}

static void __exit morsecode_exit(void)
{
	printk(KERN_INFO "<---- morse-code: exit().\n");

	// Unregister misc driver
	misc_deregister(&my_miscdevice);

	// LED:
	led_unregister();
}

// Link our init/exit functions into the kernel's code.
module_init(morsecode_init);
module_exit(morsecode_exit);

// Information about this module:
MODULE_AUTHOR("Travis and Gabriel");
MODULE_DESCRIPTION("Morse Code driver");
MODULE_LICENSE("GPL"); // Important to leave as GPL
