// SPDX-License-Identifier: GPL-2.0
/*
 * platform_gpio.c - LKSS Lab 4: LEDs via sysfs + button press/release messages
 *
 * Sysfs (under /sys/bus/platform/devices/lkss-lab4/):
 *   led0, led1, led2
 *       Write "1" -> LED on, write "0" -> LED off, read -> current state
 *
 * Buttons have no sysfs files: pressing/releasing one just prints a message
 * to the kernel log (dmesg -w).
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#define NUM_LEDS    3
#define NUM_BUTTONS 4

struct btn_ctx {
	struct device    *dev;
	struct gpio_desc *gpiod;
	int               index; /* 1-based: BTN1..BTN4 */
};

struct lab4_gpio {
	struct gpio_desc *led[NUM_LEDS];
	struct gpio_desc *btn[NUM_BUTTONS];
	struct btn_ctx    btn_ctx[NUM_BUTTONS];
};

static irqreturn_t btn_irq_handler(int irq, void *data)
{
	struct btn_ctx *ctx = data;
	int val = gpiod_get_value_cansleep(ctx->gpiod);

	dev_info(ctx->dev, "BTN%d %s\n", ctx->index, val ? "pressed" : "released");

	return IRQ_HANDLED;
}

#define DEFINE_LED_ATTR(N)						\
static ssize_t led##N##_show(struct device *dev,			\
			      struct device_attribute *attr, char *buf) \
{									\
	struct lab4_gpio *p = dev_get_drvdata(dev);			\
	return sysfs_emit(buf, "%d\n", gpiod_get_value(p->led[N]));	\
}									\
static ssize_t led##N##_store(struct device *dev,			\
			       struct device_attribute *attr,		\
			       const char *buf, size_t count)		\
{									\
	struct lab4_gpio *p = dev_get_drvdata(dev);			\
	int val;							\
									\
	if (kstrtoint(buf, 0, &val))					\
		return -EINVAL;						\
									\
	gpiod_set_value(p->led[N], val ? 1 : 0);			\
	return count;							\
}									\
static DEVICE_ATTR_RW(led##N)

DEFINE_LED_ATTR(0);
DEFINE_LED_ATTR(1);
DEFINE_LED_ATTR(2);

static struct attribute *lab4_gpio_attrs[] = {
	&dev_attr_led0.attr,
	&dev_attr_led1.attr,
	&dev_attr_led2.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lab4_gpio);

static int lab4_gpio_probe(struct platform_device *pdev)
{
	struct device    *dev = &pdev->dev;
	struct lab4_gpio *priv;
	int i, irq, ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	for (i = 0; i < NUM_LEDS; i++) {
		priv->led[i] = devm_gpiod_get_index(dev, "led", i, GPIOD_OUT_LOW);
		if (IS_ERR(priv->led[i]))
			return dev_err_probe(dev, PTR_ERR(priv->led[i]),
					      "failed to get LED %d\n", i);
	}

	for (i = 0; i < NUM_BUTTONS; i++) {
		priv->btn[i] = devm_gpiod_get_index(dev, "button", i, GPIOD_IN);
		if (IS_ERR(priv->btn[i]))
			return dev_err_probe(dev, PTR_ERR(priv->btn[i]),
					      "failed to get button %d\n", i);

		priv->btn_ctx[i].dev   = dev;
		priv->btn_ctx[i].gpiod = priv->btn[i];
		priv->btn_ctx[i].index = i + 1;

		irq = gpiod_to_irq(priv->btn[i]);
		if (irq < 0)
			return dev_err_probe(dev, irq, "no IRQ for button %d\n", i);

		ret = devm_request_threaded_irq(dev, irq, NULL, btn_irq_handler,
						 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
						 IRQF_ONESHOT,
						 "lkss-button", &priv->btn_ctx[i]);
		if (ret)
			return dev_err_probe(dev, ret,
					      "failed to request IRQ for button %d\n", i);
	}

	dev_info(dev, "probed: LED0(red) LED1(green) LED2(blue) + BTN1..BTN4\n");
	return 0;
}

static const struct of_device_id lab4_gpio_of_match[] = {
	{ .compatible = "lkss,lab4" },
	{ }
};
MODULE_DEVICE_TABLE(of, lab4_gpio_of_match);

static struct platform_driver lab4_gpio_driver = {
	.probe  = lab4_gpio_probe,
	.driver = {
		.name           = "lkss-lab4",
		.of_match_table = lab4_gpio_of_match,
		.dev_groups     = lab4_gpio_groups,
	},
};
module_platform_driver(lab4_gpio_driver);

MODULE_AUTHOR("LKSS Lab <lkss@nxp.com>");
MODULE_DESCRIPTION("LKSS Lab 4: LEDs via sysfs + button press/release messages");
MODULE_LICENSE("GPL");
