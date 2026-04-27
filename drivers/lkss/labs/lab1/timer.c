// SPDX-License-Identifier: GPL-2.0
/*
 * LKSS Lab 1 - Exercise 5: Kernel Timer demonstration
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define TIMER_DELAY_MS	1000

static struct timer_list my_timer;

static void my_timer_callback(struct timer_list *t)
{
	pr_info("timer: fired! (jiffies=%lu)\n", jiffies);

	/* Exercise 5.3: uncomment to make the timer periodic */
	/* mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMER_DELAY_MS)); */

	/* Exercise 5.4: uncomment to crash inside the callback
	 * int *p = NULL;
	 * *p = 42;
	 */
}

static int __init timer_init(void)
{
	timer_setup(&my_timer, my_timer_callback, 0);
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMER_DELAY_MS));
	pr_info("timer: armed for %d ms\n", TIMER_DELAY_MS);
	return 0;
}

static void __exit timer_exit(void)
{
	timer_delete_sync(&my_timer);
	pr_info("timer: cancelled\n");
}

module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LKSS Student");
MODULE_DESCRIPTION("Kernel timer demonstration module");
