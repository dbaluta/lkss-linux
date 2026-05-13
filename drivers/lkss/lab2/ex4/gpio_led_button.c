// SPDX-License-Identifier: GPL-2.0

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>

static const char *leds[] = { "red_led", "green_led", "blue_led" };
#define NLEDS (sizeof(leds) / sizeof(leds[0]))

/* Forward declarations of the irqhandlers to satisfy the compiler */
static irqreturn_t left_btn_irq_handler(int irq, void *dev_id);
static irqreturn_t right_btn_irq_handler(int irq, void *dev_id);

/* Array of irqhandlers */
static struct {
        const irq_handler_t handler;
        uint64_t last_tstmp;    /* Timestamp to use for debouncing */
} btn_irqs[] = {
        { .handler = left_btn_irq_handler, .last_tstmp = 0, },
        { .handler = right_btn_irq_handler, .last_tstmp = 0 },
};
#define NBUTTONS (sizeof(btn_irqs) / sizeof(btn_irqs[0]))

static struct gpio_descs *led_gpios;            /* Pointer to GPIO descriptors for the LED */
static struct gpio_desc *button_gpio[NBUTTONS]; /* Pointer to GPIO descriptors for the buttons*/

static int current_led_id = 0;                  /* Id of LED that is currently turned on */
static uint64_t debounce_jiffies = 0;

static irqreturn_t left_btn_irq_handler(int irq, void *dev_id)
{
        /* TODO 2: Use the last timestamp to return if irq went off too soon */

        pr_info("Pressed the LEFT button!\n");

        /* TODO 1: Turn off the current led */

        /* TODO 1: Turn on the previous led */

        pr_info("%s turned on!\n", leds[current_led_id]);

        /* TODO 2: Store last timestamp */

        return IRQ_HANDLED;
}

static irqreturn_t right_btn_irq_handler(int irq, void *dev_id)
{
        /* TODO 2: Use the last timestamp to return if irq went off too soon */

        pr_info("Pressed the RIGHT button!\n");

        /* TODO 1: Turn off the current led */

        /* TODO 1: Turn on the next led */

        pr_info("%s turned on!\n", leds[current_led_id]);

        /* TODO 2: Store last timestamp */

        return IRQ_HANDLED;
}

static int gpio_led_probe(struct platform_device *pdev)
{
        dev_info(&pdev->dev, "gpio_led_probe()\n");

        /**
         *  TODO 1: Get LED GPIOs using devm_gpiod_get_array()
         *  Pay attention to the type this returns.
         */

        if (IS_ERR(led_gpios)) {
                pr_info("LED GPIO probe failed\n");
                return PTR_ERR(led_gpios);
        }

        return 0;
}

static int gpio_button_probe(struct platform_device *pdev)
{
        int ret, irq;
        int i;

        dev_info(&pdev->dev, "gpio_button_probe()\n");

        /* TODO 2: Setup debounce_jiffies to 200ms */

        for (i = 0; i < NBUTTONS; i++) {
                /* TODO 1: Get BTN GPIOs using devm_gpiod_get_index() */

                if (IS_ERR(button_gpio[i])) {
                        dev_err(&pdev->dev, "Failed to get GPIO\n");
                        return PTR_ERR(button_gpio[i]);
                }

                /* TODO 1: Get irq number (Hint: gpiod_to_irq) */

                /* TODO 1: Request IRQ and bind proper handler (Hint: devm_request_irq) */

                if (ret < 0) {
                        dev_err(&pdev->dev, "Failed to request_irq\n");
                        return ret;
                }
        }

        return 0;
}

static int gpios_probe(struct platform_device *pdev)
{
        int ret;

        ret = gpio_led_probe(pdev);
        if (ret)
                return ret;

        return gpio_button_probe(pdev);
}

static void gpios_remove(struct platform_device *pdev)
{
        dev_info(&pdev->dev, "gpio_button_remove()\n");
}

static const struct of_device_id gpio_button_led_dt_ids[] = {
	{ .compatible = "lkss,lab2", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, gpio_button_led_dt_ids);

static struct platform_driver gpio_button_driver = {
	.probe = gpios_probe,
	.remove = gpios_remove,
	.driver = {
		.name = "gpio_button_led",
		.of_match_table = gpio_button_led_dt_ids,
	},
};

module_platform_driver(gpio_button_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NXP Linux Kernel Summer School");
MODULE_DESCRIPTION("Lab2 Ex4: Using buttons with irqs for LED ctrl");
