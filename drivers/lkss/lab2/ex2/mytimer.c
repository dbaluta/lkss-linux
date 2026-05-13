// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>	/* Core header for loading LKMs into the kernel */
#include <linux/fs.h>		/* File operations structure and related functions */
#include <linux/miscdevice.h>	/* Misc device support */
#include <linux/kstrtox.h>	/* String parsing */
#include <linux/uaccess.h>	/* Functions for user space/kernel space access */

/* Define device name and class name */
#define DEVICE_NAME     "mytimer"
#define CLASS_NAME      "mytimer_class"
#define MAX_BUF_SIZE    256

static int major;                                  /* Major number assigned to the driver */
static char buf[MAX_BUF_SIZE];                     /* Buffer to store user data */
static struct class *mytimer_class;                /* Device class pointer */
static struct timer_list timer;
static uint64_t start;                             /* Store starting time of the timer (jiffies) */

/* List declaration - this is the HEAD node */
LIST_HEAD(my_list);

/* Entry in my_list. Has to store a ptr to the next/prev elements (list_head) */
struct timer_list_entry {
        int timeout;    /* in seconds */
        struct list_head list;
};

/**
 * Convert string to signed integer
 *
 * @param buf string to convert
 * @param out resulted integer
 *
 * @return int conversion exit code
 */
static int string_to_int(char *buf, int *out)
{
        /* TODO: Convert string to int (Hint: <linux/kstrtox.h>) */

        return 0;
}

/**
 * Inserts an entry of type timer_list_entry into
 * my_list. The entry is populated with an timeout.
 *
 * @param timeout value to be inserted
 */
static void insert_timeout(int timeout)
{
        struct timer_list_entry *entry = kmalloc(sizeof(*entry), GFP_KERNEL);

        if (!entry)
                return;

        /* TODO: populate entry and add it to the list using list_add_tail */
}

/**
 * Consume the last entry from the list and
 * return the value it contained.
 *
 * @return int value of timeout
 */
static int get_next_timeout(void)
{
        struct list_head *head, *tmp;
        struct timer_list_entry *entry;
        int timeout = 0;

        /* We use this method so that we can also delete the entry */

        list_for_each_safe(head, tmp, &my_list) {
                entry = list_entry(head, struct timer_list_entry, list);
                timeout = entry->timeout;

                /* TODO: use list_del to delete the entry (head ptr) */

                /* TODO: free entry using kfree */

                /* Only get first one */
                return timeout;
        }

        return timeout;
}

/**
 * Delete any remaining nodes in the list
 */
static void destroy_list(void)
{
        struct list_head *head, *tmp;
        struct timer_list_entry *entry;

        list_for_each_safe(head, tmp, &my_list) {
                /* TODO: Delete every entry - similar to get_next_timeout */
        }
}

static void timer_cb(struct timer_list *t)
{
        int timeout = 0;

        /* TODO: print the elapsed time in seconds (use the saved start time) */

        /* TODO: Get next timeout from list */
        timeout = get_next_timeout();

        /*
         * TODO: Restart timer if timeout != 0 (Don't forget to save the start
         * time)
         */
}

/**
 * Called when the device is opened
 */
static int dev_open(struct inode *inodep, struct file *filep) {
        return 0;
}

/**
 * Called when the device is closed
 */
static int dev_release(struct inode *inodep, struct file *filep) {
        return 0;
}

/**
 * Called when user writes to the device
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
        int err;
        int timeout;

        /* Limit message length to prevent overflow */
        len = MIN(len, 255);

        /* TODO: Copy data from user space to kernel buffer (the timeout) */


        buf[len] = '\0';

        /* TODO: Parse the string - use string_to_int */

        if (timer_pending(&timer)) {
                /* TODO: If timer is running, postpone timeout using list */
                insert_timeout(timeout);
                return len;
        }

        if (timeout) {
                /* TODO: Save current time and start timer */
        }

        return len;/* Return number of bytes written */
}


/**
 * File operations structure for this driver
 */
static struct file_operations fops = {
	.open = dev_open,
	.write = dev_write,
	.release = dev_release
};

/*
 * TODO: Declare miscdevice and name it 'mytimer'.
 * Use file ops above. Get a dynamically assigned minor.
 */

/**
 * Initialization function for the module
 */
static int __init mytimer_init(void) {
        int minor = 0;

        /* Initialize timer */
        timer_setup(&timer, timer_cb, 0);

        /* TODO: Register misc device */

        /* TODO: Get dynamically assigned minor */

        pr_info("%s: Device initialized, minor %d\n", DEVICE_NAME, minor);
        return 0;
}

/**
 * Cleanup function for the module
 */
static void __exit mytimer_exit(void) {
        /* TODO: Unregister misc device */

        /* Delete timer */
        timer_delete_sync(&timer);

        destroy_list();

        pr_info("%s: Goodbye!\n", DEVICE_NAME);
}

module_init(mytimer_init);
module_exit(mytimer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Linux Kernel Summer School");
MODULE_DESCRIPTION("Lab2 Ex2: Command a timer using a char device");
