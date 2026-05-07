// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_gpio.c – LKSS lab platform driver: 3 LEDs + 4 buttons via GPIO
 *
 * This module binds to the "lkss,gpio-demo" compatible node defined in
 * imx93-11x11-frdm-lkss.dts.  It demonstrates the core kernel APIs that
 * every GPIO-based driver uses:
 *
 *   • Platform driver registration  (platform_driver / module_platform_driver)
 *   • Device Tree binding           (of_match_table / devm_gpiod_get_index)
 *   • GPIO descriptor API           (gpiod_direction_*, gpiod_set/get_value)
 *   • Edge-triggered interrupts     (gpiod_to_irq / devm_request_irq)
 *   • Managed device resources      (devm_* – automatic cleanup on unbind)
 *   • Sysfs attributes              (DEVICE_ATTR_RW/RO / dev_groups)
 *
 * Sysfs interface (under /sys/bus/platform/devices/lkss-gpio/):
 *
 *   led0, led1, led2
 *       Read  → current output level (0 or 1)
 *       Write → "0" turns LED off, "1" turns LED on
 *
 *   button0, button1, button2, button3
 *       Read  → logical value: 1 = button pressed, 0 = released
 *       (The gpiod layer inverts the physical active-low line automatically.)
 *
 * Every button press/release also emits a dev_info() message visible with:
 *       dmesg -w
 *
 * Cross-compile:
 *   make -C /path/to/linux M=$(pwd) ARCH=arm64 \
 *        CROSS_COMPILE=aarch64-linux-gnu- modules
 *
 * Hardware:
 *   LED0 (red):   GPIO2_IO07 / EXT2 pin 26  (active-high, 220 Ω resistor)
 *   LED1 (green): GPIO2_IO04 / EXT2 pin  7  (active-high, 220 Ω resistor)
 *   LED2 (blue):  GPIO2_IO18 / EXT2 pin 12  (active-high, 220 Ω resistor)
 *   BTN1:         GPIO2_IO05 / EXT2 pin 29  (active-low, 10 kΩ pull-up)
 *   BTN2:         GPIO2_IO06 / EXT2 pin 31  (active-low, 10 kΩ pull-up)
 *   BTN3:         GPIO2_IO00 / EXT2 pin 27  (active-low, 10 kΩ pull-up)
 *   BTN4:         GPIO2_IO01 / EXT2 pin 28  (active-low, 10 kΩ pull-up)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>   /* gpiod API */
#include <linux/interrupt.h>       /* request_irq, IRQF_* */
#include <linux/of.h>              /* of_match_table */
#include <linux/slab.h>

/* ── Constants ───────────────────────────────────────────────────────────── */

#define NUM_LEDS    3
#define NUM_BUTTONS 4

/* ── Driver private data ─────────────────────────────────────────────────── */

/*
 * Per-button context passed as the 'data' argument to the IRQ handler.
 * We allocate one of these per button so the handler knows which GPIO fired.
 */
struct btn_ctx {
	struct device    *dev;   /* for dev_info() / dev_err() in IRQ context */
	struct gpio_desc *gpiod; /* to read the line level after the edge      */
	int               index; /* 0–3, matches the DT button-gpios order     */
};

/*
 * Main per-device state structure, allocated by probe() and stored via
 * platform_set_drvdata() so sysfs callbacks can retrieve it with
 * dev_get_drvdata().
 */
struct lkss_gpio {
	struct gpio_desc *led[NUM_LEDS];
	struct gpio_desc *btn[NUM_BUTTONS];
	struct btn_ctx    btn_ctx[NUM_BUTTONS]; /* one ctx per button IRQ */
};

/* ── Interrupt handler ───────────────────────────────────────────────────── */

/*
 * Called on both rising and falling edges of each button GPIO.
 * Reading the GPIO value after the edge tells us whether the button is
 * currently pressed (logical 1 because GPIOD inverts active-low) or released.
 *
 * NOTE: gpiod_get_value_cansleep() is safe here because IRQF_TRIGGER_BOTH
 * is handled in a threaded context when the underlying GPIO controller
 * needs a sleeping I²C/SPI read.  If the GPIO controller is memory-mapped
 * (which it is on i.MX93), the non-sleeping variant works too; we use the
 * _cansleep variant defensively.
 */
static irqreturn_t lkss_btn_irq(int irq, void *data)
{
	struct btn_ctx *ctx = data;
	int val = gpiod_get_value_cansleep(ctx->gpiod);

	/*
	 * val == 1  →  button pressed  (gpiod has inverted the active-low line)
	 * val == 0  →  button released
	 */
	dev_info(ctx->dev, "button%d %s\n",
	         ctx->index, val ? "pressed" : "released");

	return IRQ_HANDLED;
}

/* ── Sysfs attributes ────────────────────────────────────────────────────── */

/*
 * DEFINE_LED_ATTR(N) expands to the show/store functions and the
 * DEVICE_ATTR_RW declaration for led<N>.
 *
 * show  → returns the current logical output level ("0\n" or "1\n")
 * store → accepts "0" or "1" and drives the GPIO accordingly
 */
#define DEFINE_LED_ATTR(N)						\
static ssize_t led##N##_show(struct device *dev,			\
			     struct device_attribute *attr, char *buf)	\
{									\
	struct lkss_gpio *p = dev_get_drvdata(dev);			\
	return sysfs_emit(buf, "%d\n", gpiod_get_value(p->led[N]));	\
}									\
static ssize_t led##N##_store(struct device *dev,			\
			      struct device_attribute *attr,		\
			      const char *buf, size_t count)		\
{									\
	struct lkss_gpio *p = dev_get_drvdata(dev);			\
	int val;							\
	if (kstrtoint(buf, 0, &val))					\
		return -EINVAL;						\
	gpiod_set_value(p->led[N], !!val);				\
	return count;							\
}									\
static DEVICE_ATTR_RW(led##N)

DEFINE_LED_ATTR(0);
DEFINE_LED_ATTR(1);
DEFINE_LED_ATTR(2);

/*
 * DEFINE_BTN_ATTR(N) creates a read-only sysfs file for button<N>.
 * Returns "1\n" when pressed, "0\n" when released.
 * The gpiod layer handles the active-low inversion transparently.
 */
#define DEFINE_BTN_ATTR(N)						\
static ssize_t button##N##_show(struct device *dev,			\
				struct device_attribute *attr,		\
				char *buf)				\
{									\
	struct lkss_gpio *p = dev_get_drvdata(dev);			\
	return sysfs_emit(buf, "%d\n", gpiod_get_value(p->btn[N]));	\
}									\
static DEVICE_ATTR_RO(button##N)

DEFINE_BTN_ATTR(0);
DEFINE_BTN_ATTR(1);
DEFINE_BTN_ATTR(2);
DEFINE_BTN_ATTR(3);

/*
 * Collect all attributes into a group.  The ATTRIBUTE_GROUPS() macro builds
 * lkss_gpio_groups[], which is assigned to driver.dev_groups below so the
 * kernel creates/removes the sysfs files automatically on probe/remove.
 */
static struct attribute *lkss_gpio_attrs[] = {
	&dev_attr_led0.attr,
	&dev_attr_led1.attr,
	&dev_attr_led2.attr,
	&dev_attr_button0.attr,
	&dev_attr_button1.attr,
	&dev_attr_button2.attr,
	&dev_attr_button3.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lkss_gpio);

/* ── Probe ───────────────────────────────────────────────────────────────── */

/*
 * probe() is called once when the platform bus matches the DT node's
 * compatible string against our of_match_table.
 *
 * All allocations and registrations use the devm_ (device-managed) variants.
 * When the driver is unbound (or the module is removed), the kernel releases
 * every devm_ resource automatically in reverse order — no remove() needed.
 */
static int lkss_gpio_probe(struct platform_device *pdev)
{
	struct device    *dev = &pdev->dev;
	struct lkss_gpio *priv;
	int i, irq, ret;

	/* Allocate zeroed private data, lifetime tied to device */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	/* ── LEDs: request as outputs, initially off ── */
	for (i = 0; i < NUM_LEDS; i++) {
		/*
		 * "led" matches the "led-gpios" DT property (the suffix "-gpios"
		 * is the convention; the driver just uses the base name "led").
		 * GPIOD_OUT_LOW initialises the output to the inactive state
		 * (logical 0 → physical low for ACTIVE_HIGH).
		 */
		priv->led[i] = devm_gpiod_get_index(dev, "led", i,
						     GPIOD_OUT_LOW);
		if (IS_ERR(priv->led[i])) {
			dev_err(dev, "failed to request LED %d: %ld\n",
				i, PTR_ERR(priv->led[i]));
			return PTR_ERR(priv->led[i]);
		}
		gpiod_set_consumer_name(priv->led[i], "lkss-led");
	}

	/* ── Buttons: request as inputs, set up edge IRQs ── */
	for (i = 0; i < NUM_BUTTONS; i++) {
		priv->btn[i] = devm_gpiod_get_index(dev, "button", i,
						     GPIOD_IN);
		if (IS_ERR(priv->btn[i])) {
			dev_err(dev, "failed to request button %d: %ld\n",
				i, PTR_ERR(priv->btn[i]));
			return PTR_ERR(priv->btn[i]);
		}
		gpiod_set_consumer_name(priv->btn[i], "lkss-button");

		/* Populate per-button context for the IRQ handler */
		priv->btn_ctx[i].dev   = dev;
		priv->btn_ctx[i].gpiod = priv->btn[i];
		priv->btn_ctx[i].index = i;

		/*
		 * gpiod_to_irq() translates the GPIO descriptor to a Linux
		 * IRQ number managed by the GPIO controller's irqchip.
		 */
		irq = gpiod_to_irq(priv->btn[i]);
		if (irq < 0) {
			dev_err(dev, "no IRQ for button %d\n", i);
			return irq;
		}

		/*
		 * Request both edges so we detect press and release.
		 * devm_request_threaded_irq() spawns a kernel thread for the
		 * handler, which is required if gpiod_get_value_cansleep()
		 * is used inside it (not strictly needed for MMIO GPIO but
		 * safer and more portable).
		 */
		ret = devm_request_threaded_irq(dev, irq,
						NULL,           /* hard IRQ: none */
						lkss_btn_irq,   /* thread fn     */
						IRQF_TRIGGER_RISING  |
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"lkss-button",
						&priv->btn_ctx[i]);
		if (ret) {
			dev_err(dev, "failed to request IRQ %d for button %d: %d\n",
				irq, i, ret);
			return ret;
		}
	}

	dev_info(dev, "probed: %d LEDs, %d buttons\n", NUM_LEDS, NUM_BUTTONS);
	dev_info(dev, "LEDs:    /sys/bus/platform/devices/lkss-gpio/led{0,1,2}\n");
	dev_info(dev, "Buttons: /sys/bus/platform/devices/lkss-gpio/button{0,1,2,3}\n");

	return 0;
}

/* ── Device Tree match table ─────────────────────────────────────────────── */

static const struct of_device_id lkss_gpio_of_match[] = {
	{ .compatible = "lkss,gpio-demo" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lkss_gpio_of_match);

/* ── Platform driver structure ───────────────────────────────────────────── */

static struct platform_driver lkss_gpio_driver = {
	.probe  = lkss_gpio_probe,
	/* No .remove() needed – all resources are devm-managed */
	.driver = {
		.name           = "lkss-gpio",
		.of_match_table = lkss_gpio_of_match,
		/*
		 * dev_groups registers the sysfs attribute group with the
		 * driver core; files are created after probe() returns 0
		 * and removed automatically on unbind.
		 */
		.dev_groups     = lkss_gpio_groups,
	},
};

/*
 * module_platform_driver() is a convenience macro that expands to
 * module_init() / module_exit() wrappers calling
 * platform_driver_register() and platform_driver_unregister().
 */
module_platform_driver(lkss_gpio_driver);

MODULE_AUTHOR("LKSS Lab <lkss@example.com>");
MODULE_DESCRIPTION("LKSS lab driver: 3 LED GPIOs + 4 button GPIOs with sysfs");
MODULE_LICENSE("GPL");
