// SPDX-License-Identifier: GPL-2.0
/*
 * LKSS Lab 1 - Exercise 4: Kernel Oops demonstration
 *
 * This module deliberately dereferences a NULL pointer inside its init
 * function, triggering a kernel oops.  Load it with insmod and observe the
 * output on the serial console and in dmesg.
 *
 */

#include <linux/module.h>
#include <linux/init.h>

static int __init oops_sample_init(void)
{
	int *p = NULL;

	pr_info("oops: about to dereference NULL pointer...\n");
	*p = 42;	/* NULL dereference – triggers kernel oops */

	return 0;
}

static void __exit oops_sample_exit(void)
{
	pr_info("oops: exit\n");
}

module_init(oops_sample_init);
module_exit(oops_sample_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LKSS Student");
MODULE_DESCRIPTION("Kernel oops demonstration module");
