// SPDX-License-Identifier: GPL-2.0
/*
 * LKSS Lab 1 - Exercise 1: Hello World kernel module
 *
 */

#include <linux/module.h>	/* module_init, module_exit, MODULE_* macros */
#include <linux/init.h>		/* __init, __exit */

static int __init hello_init(void)
{
	pr_info("Hello, kernel!\n");
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("Goodbye, kernel!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LKSS Student");
MODULE_DESCRIPTION("Hello World kernel module");
