// SPDX-License-Identifier: GPL-2.0

#include <linux/of.h>			/* For Open Firmware (Device Tree) support */
#include <linux/platform_device.h>	/* For platform device/driver APIs */
#include <linux/cdev.h>			/* For character device support */
#include <linux/gpio/consumer.h>	/* For GPIO descriptor (gpiod_*) API */
#include <linux/ioctl.h>		/* For ioctl macros */

/* Name of the character device node */
#define DEVICE_NAME "gpio_led"

static const char *leds[] = { "red_led", "green_led", "blue_led" };
#define NLEDS (sizeof(leds) / sizeof(leds[0]))

#ifdef CONFIG_LKSS_DRIVERS_LAB2_EX3_IOCTL
#define GPIO_LED_MAGIC	'L'			/* Unique magic number for IOCTL */
/* TODO 2: IOCTL command to turn LED on */
/* TODO 2: IOCTL command to turn LED off */
#endif

static struct gpio_desc *led_gpio[NLEDS];	/* Pointer to GPIO descriptor for the LED */
static dev_t dev_num;   			/* Device number for character device */
static struct cdev my_cdev[NLEDS];		/* Character device structure */
static struct class *led_class;		        /* Device class for sysfs entry */

static int get_minor(struct file *filep)
{
        struct cdev *cdev;
        int minor = -1;

        /* TODO 1: Get cdev from filep (remember inodes) */

        /* TODO 1: Get device minor from cdev->dev */

        return minor;
}

/**
 * Write function: control LED (on/off)
 */
static ssize_t gpio_led_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
        char kbuf[2];   /* Kernel buffer to store one char and null terminator */
        int minor;

        if (copy_from_user(kbuf, buf, 1))       /* Copy 1 byte from user space */
                return -EFAULT;

        kbuf[1] = '\0'; /* Null-terminate the string */

        /* TODO 1: Get device minor */

        /* TODO 1: Turn led ON/OFF based of char in kbuf */

        return count;/* Return number of bytes written */
}

/**
 *  Read function: get LED state ("on\n" or "off\n")
 */
static ssize_t gpio_led_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
        const char *state_str;/* String to return LED state */
        size_t len;           /* Length of the string */
        int state;            /* Current GPIO state */
        int minor;

        /* Return 0 on subsequent reads (EOF) */
        if (*ppos > 0)
                return 0;

        /* TODO 1: Get device minor */

        /* TODO 1: Get GPIO state */

        if (state)/* Convert value to string */
                state_str = "on\n";
        else
                state_str = "off\n";

        /* Length including null terminator */
        len = strlen(state_str) + 1;

        /* Copy state string to user space */
        if (copy_to_user(buf, state_str, len))
                return -EFAULT;

        /* Update file position to mark EOF */
        *ppos = len;

        /* Return number of bytes read */
        return len;
}

/**
 * IOCTL handler for user-space commands
 *
 * @param file is the file pointer to the file that was passed by the application
 * @param cmd is the ioctl command that was called from the userspace
 * @param arg are the arguments passed from the userspace
 */
#ifdef CONFIG_LKSS_DRIVERS_LAB2_EX3_IOCTL
static long gpio_led_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        int value;
        int minor;

        /* TODO 1: Get minor */

        /* TODO 1: Use a switch statement to handle LED_ON and LED_OFF, use gpiod_set_value() */


        return 0; /* Success */
}
#endif

/**
 * File operations structure for the character device
 */
static struct file_operations fops = {
	.owner = THIS_MODULE,			/* Points to the current module */
	.write = gpio_led_write,		/* Assign write handler */
	.read = gpio_led_read,			/* Assign read handler */
#ifdef CONFIG_LKSS_DRIVERS_LAB2_EX3_IOCTL
	.unlocked_ioctl = gpio_led_ioctl,	/* IOCTL operation */
#endif
};

/**
 * Probe function: called when the driver is matched with a device
 */
static int gpio_led_probe(struct platform_device *pdev)
{
        int ret;
        int major;
        int i;

        /* TODO 1: Allocate char devices dynamically - alloc_chrdev_region */

        if (ret)
                return ret;

        /* TODO 1: Get device major */

        /* Create device class in sysfs (/sys/class) */
        led_class = class_create("led_class");
        if (IS_ERR(led_class)) {
                ret = PTR_ERR(led_class);
                goto err_del_cdev;
        }

        /* TODO 1: Request GPIO associated with device node (default: output low) */
        for (i = 0; i < NLEDS; i++) {
                /* TODO 1: Use gpiod_get_index to get the led gpio */

                if (IS_ERR(led_gpio[i])) {
                        dev_err(&pdev->dev, "Failed to get GPIO\n");
                        return PTR_ERR(led_gpio[i]);
                }
                /* TODO 1: Initialize character device with file operations (use cdev_init) */


                my_cdev[i].owner = THIS_MODULE;

                /* TODO 1: Add character device to the system (cdev_add) */

                if (ret)
                        goto err_unregister_chrdev;

                /* TODO 1: Create device node in /dev (e.g., /dev/<color>_led) */

        }

        dev_info(&pdev->dev, "LED driver loaded\n");

        return 0;

        /* Error handling (reverse order of allocation) */
err_del_cdev:
        for (i = 0; i < NLEDS; i++)
                cdev_del(&my_cdev[i]);
err_unregister_chrdev:
        unregister_chrdev_region(dev_num, NLEDS);
err_free_gpio:
        for (i = 0; i < NLEDS && led_gpio[i]; i++)
                gpiod_put(led_gpio[i]);

        return ret;
}

/**
 * Remove function: cleanup when device is removed/unregistered
 */
static void gpio_led_remove(struct platform_device *pdev)
{
        int i;
        device_destroy(led_class, dev_num);	/* Remove /dev/<color>_led */
        class_destroy(led_class);		/* Remove class from /sys/class */
        for (i = 0; i < NLEDS; i++)
                cdev_del(&my_cdev[i]);			/* Remove character device */
        unregister_chrdev_region(dev_num, NLEDS);	/* Free device number */
        for (i = 0; i < NLEDS; i++)
                gpiod_put(led_gpio[i]);			/* Release GPIO */
}

/**
 * Device tree match table
 */
static const struct of_device_id gpio_led_dt_ids[] = {
	{ .compatible = "lkss,lab2", }, /* Matches with Device Tree entry */
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, gpio_led_dt_ids);/* Expose match table to userspace */

/**
 * Platform driver structure
 */
static struct platform_driver gpio_led_driver = {
	.probe = gpio_led_probe,        /* Called when device is found */
	.remove = gpio_led_remove,      /* Called when device is removed */
	.driver = {
		.name = "gpio-led-ioctl",         /* Name of the driver */
		.of_match_table = gpio_led_dt_ids,/* Device Tree matching */
	},
};

module_platform_driver(gpio_led_driver);/* Register platform driver */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Linux Kernel Summer School");
MODULE_DESCRIPTION("Lab2 Ex4: Character device LED driver using gpiod and ioctl() commands");
