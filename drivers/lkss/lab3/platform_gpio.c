// SPDX-License-Identifier: GPL-2.0
/*
 * platform_gpio.c - LKSS Lab 3, Exercise 12
 *
 * Platform driver for the LKSS daughter board GPIO interface:
 *   4 push-buttons (active-low, pulled up)
 *   3 LEDs         (active-high)
 *
 * Exposes sysfs attributes on the platform device:
 *   button0 .. button3   read-only:  "0\n" released, "1\n" pressed
 *   led0    .. led2      read/write: write "1" on, "0" off
 *
 * On module load the driver prints the sysfs path so users know where to
 * find the attributes.
 *
 * Device tree node (add to imx93-11x11-frdm.dts):
 *
 *   lkss_gpio: lkss-gpio {
 *       compatible = "lkss,platform-gpio";
 *
 *       button-gpios = <&gpio1 0 GPIO_ACTIVE_LOW>,
 *                      <&gpio1 1 GPIO_ACTIVE_LOW>,
 *                      <&gpio1 2 GPIO_ACTIVE_LOW>,
 *                      <&gpio1 3 GPIO_ACTIVE_LOW>;
 *
 *       led-gpios    = <&gpio1 4 GPIO_ACTIVE_HIGH>,
 *                      <&gpio1 5 GPIO_ACTIVE_HIGH>,
 *                      <&gpio1 6 GPIO_ACTIVE_HIGH>;
 *   };
 *
 * Adjust GPIO bank/pin numbers to match the LKSS daughter board schematic.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/kobject.h>

#define NBUTTONS 4
#define NLEDS    3

struct lkss_gpio_priv {
	struct gpio_desc *buttons[NBUTTONS];
	struct gpio_desc *leds[NLEDS];
};

/* ---- Button sysfs show -------------------------------------------------- */

static ssize_t button0_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->buttons[0]));
}
static DEVICE_ATTR_RO(button0);

static ssize_t button1_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->buttons[1]));
}
static DEVICE_ATTR_RO(button1);

static ssize_t button2_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->buttons[2]));
}
static DEVICE_ATTR_RO(button2);

static ssize_t button3_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->buttons[3]));
}
static DEVICE_ATTR_RO(button3);

/* ---- LED sysfs show / store --------------------------------------------- */

static ssize_t led0_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->leds[0]));
}

static ssize_t led0_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	gpiod_set_value(priv->leds[0], !!val);
	return count;
}
static DEVICE_ATTR_RW(led0);

static ssize_t led1_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->leds[1]));
}

static ssize_t led1_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	gpiod_set_value(priv->leds[1], !!val);
	return count;
}
static DEVICE_ATTR_RW(led1);

static ssize_t led2_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gpiod_get_value(priv->leds[2]));
}

static ssize_t led2_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct lkss_gpio_priv *priv = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	gpiod_set_value(priv->leds[2], !!val);
	return count;
}
static DEVICE_ATTR_RW(led2);

/* ---- Attribute group ---------------------------------------------------- */

static struct attribute *lkss_gpio_attrs[] = {
	&dev_attr_button0.attr,
	&dev_attr_button1.attr,
	&dev_attr_button2.attr,
	&dev_attr_button3.attr,
	&dev_attr_led0.attr,
	&dev_attr_led1.attr,
	&dev_attr_led2.attr,
	NULL,
};

static const struct attribute_group lkss_gpio_group = {
	.attrs = lkss_gpio_attrs,
};

/* ---- Probe / remove ----------------------------------------------------- */

static int lkss_gpio_probe(struct platform_device *pdev)
{
	struct lkss_gpio_priv *priv;
	char *path;
	int i, ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	for (i = 0; i < NBUTTONS; i++) {
		priv->buttons[i] = devm_gpiod_get_index(&pdev->dev, "button",
							 i, GPIOD_IN);
		if (IS_ERR(priv->buttons[i])) {
			dev_err(&pdev->dev, "failed to get button%d GPIO\n", i);
			return PTR_ERR(priv->buttons[i]);
		}
	}

	for (i = 0; i < NLEDS; i++) {
		priv->leds[i] = devm_gpiod_get_index(&pdev->dev, "led",
						      i, GPIOD_OUT_LOW);
		if (IS_ERR(priv->leds[i])) {
			dev_err(&pdev->dev, "failed to get led%d GPIO\n", i);
			return PTR_ERR(priv->leds[i]);
		}
	}

	ret = devm_device_add_group(&pdev->dev, &lkss_gpio_group);
	if (ret) {
		dev_err(&pdev->dev, "failed to create sysfs group: %d\n", ret);
		return ret;
	}

	path = kobject_get_path(&pdev->dev.kobj, GFP_KERNEL);
	dev_info(&pdev->dev, "lkss_gpio: sysfs path: /sys%s\n",
		 path ? path : "(unknown)");
	dev_info(&pdev->dev, "lkss_gpio: buttons:    /sys%s/button[0-3]\n",
		 path ? path : "");
	dev_info(&pdev->dev, "lkss_gpio: LEDs:       /sys%s/led[0-2]\n",
		 path ? path : "");
	kfree(path);

	return 0;
}

static void lkss_gpio_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "lkss_gpio removed\n");
}

/* ---- Driver registration ------------------------------------------------ */

static const struct of_device_id lkss_gpio_of_match[] = {
	{ .compatible = "lkss,platform-gpio" },
	{ }
};
MODULE_DEVICE_TABLE(of, lkss_gpio_of_match);

static struct platform_driver lkss_gpio_driver = {
	.probe  = lkss_gpio_probe,
	.remove = lkss_gpio_remove,
	.driver = {
		.name           = "lkss-platform-gpio",
		.of_match_table = lkss_gpio_of_match,
	},
};
module_platform_driver(lkss_gpio_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("LKSS Lab 3 Exercise 12: Platform GPIO driver for buttons and LEDs");
MODULE_LICENSE("GPL v2");
