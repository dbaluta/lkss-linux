// SPDX-License-Identifier: GPL-2.0
/*
 * platform_gpio.c – LKSS Lab 4: LED blinking + button interrupt handlers
 *
 * Demonstrates:
 *   • Platform driver registration  (module_platform_driver)
 *   • Device Tree binding           (of_match_table / devm_gpiod_get_index)
 *   • GPIO descriptor API           (gpiod_direction_*, gpiod_set/get_value)
 *   • Kernel timers                 (timer_setup, mod_timer, timer_delete_sync)
 *   • Edge-triggered IRQs           (gpiod_to_irq, devm_request_threaded_irq)
 *   • Sysfs attributes              (DEVICE_ATTR_RW/RO, ATTRIBUTE_GROUPS)
 *   • Managed resources             (devm_* – automatic cleanup on unbind)
 *
 * Sysfs (under /sys/bus/platform/devices/lkss-lab4/):
 *
 *   led0, led1, led2
 *       Write "1" → start blinking (toggles every BLINK_MS milliseconds)
 *       Write "0" → stop blinking and turn LED off
 *       Read       → "1" if blinking, "0" if off
 *
 *   button0, button1, button2, button3
 *       Read  → "1" if pressed, "0" if released
 *
 * Button edges also print to the kernel log — observe with:  dmesg -w
 *
 * Hardware: FRDM-IMX93 EXT2 header (J601)
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
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/of.h>
#include <linux/slab.h>

/* ── Constants ───────────────────────────────────────────────────────────── */

#define NUM_LEDS    3
#define NUM_BUTTONS 4
#define BLINK_MS    500   /* LED toggle interval in milliseconds */

/* ── Per-LED blink state ─────────────────────────────────────────────────── */

/*
 * Each LED owns a timer_list.  The timer fires in softirq context and
 * toggles the GPIO, then re-arms itself.  Only non-sleeping GPIO ops are
 * safe here; memory-mapped GPIO on i.MX93 never sleeps so gpiod_get_value /
 * gpiod_set_value are fine without the _cansleep variants.
 */
struct led_blink {
	struct gpio_desc  *gpiod;
	struct timer_list  timer;
	bool               blinking;
};

/* ── Per-button IRQ context ──────────────────────────────────────────────── */

struct btn_ctx {
	struct device    *dev;
	struct gpio_desc *gpiod;
	int               index;  /* 1-based to match BTN1..BTN4 labels */
};

/* ── Driver private data ─────────────────────────────────────────────────── */

struct lab4_gpio {
	struct led_blink  led[NUM_LEDS];
	struct gpio_desc *btn[NUM_BUTTONS];
	struct btn_ctx    btn_ctx[NUM_BUTTONS];
};

/* ── Timer callback ──────────────────────────────────────────────────────── */

static void led_blink_fn(struct timer_list *t)
{
	struct led_blink *lb = timer_container_of(lb, t, timer);

	gpiod_set_value(lb->gpiod, !gpiod_get_value(lb->gpiod));
	mod_timer(&lb->timer, jiffies + msecs_to_jiffies(BLINK_MS));
}

/* ── Button IRQ handler ──────────────────────────────────────────────────── */

/*
 * Threaded IRQ — runs in a kernel thread, safe to call _cansleep variants.
 * gpiod handles active-low inversion: logical 1 == button pressed.
 */
static irqreturn_t btn_irq_handler(int irq, void *data)
{
	struct btn_ctx *ctx = data;
	int val = gpiod_get_value_cansleep(ctx->gpiod);

	dev_info(ctx->dev, "BTN%d %s\n",
		 ctx->index, val ? "pressed" : "released");

	return IRQ_HANDLED;
}

/* ── Sysfs: led{0,1,2} ───────────────────────────────────────────────────── */

/*
 * DEFINE_LED_ATTR(N) generates show/store callbacks and DEVICE_ATTR_RW for
 * the sysfs file "led<N>".
 *
 *   show  → "1\n" if blinking, "0\n" if off
 *   store → "1" arms the timer; "0" disarms and drives GPIO low
 */
#define DEFINE_LED_ATTR(N)						\
static ssize_t led##N##_show(struct device *dev,			\
			     struct device_attribute *attr, char *buf)	\
{									\
	struct lab4_gpio *p = dev_get_drvdata(dev);			\
	return sysfs_emit(buf, "%d\n", p->led[N].blinking ? 1 : 0);	\
}									\
static ssize_t led##N##_store(struct device *dev,			\
			      struct device_attribute *attr,		\
			      const char *buf, size_t count)		\
{									\
	struct lab4_gpio *p = dev_get_drvdata(dev);			\
	int val;							\
	if (kstrtoint(buf, 0, &val))					\
		return -EINVAL;						\
	if (val) {							\
		if (!p->led[N].blinking) {				\
			p->led[N].blinking = true;			\
			mod_timer(&p->led[N].timer,			\
				  jiffies + msecs_to_jiffies(BLINK_MS)); \
		}							\
	} else {							\
		p->led[N].blinking = false;				\
		timer_delete_sync(&p->led[N].timer);			\
		gpiod_set_value(p->led[N].gpiod, 0);			\
	}								\
	return count;							\
}									\
static DEVICE_ATTR_RW(led##N)

DEFINE_LED_ATTR(0);
DEFINE_LED_ATTR(1);
DEFINE_LED_ATTR(2);

/* ── Sysfs: button{0,1,2,3} ─────────────────────────────────────────────── */

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
	struct lab4_gpio *p = dev_get_drvdata(dev);			\
	return sysfs_emit(buf, "%d\n",					\
			  gpiod_get_value(p->btn[N]));			\
}									\
static DEVICE_ATTR_RO(button##N)

DEFINE_BTN_ATTR(0);
DEFINE_BTN_ATTR(1);
DEFINE_BTN_ATTR(2);
DEFINE_BTN_ATTR(3);

static struct attribute *lab4_gpio_attrs[] = {
	&dev_attr_led0.attr,
	&dev_attr_led1.attr,
	&dev_attr_led2.attr,
	&dev_attr_button0.attr,
	&dev_attr_button1.attr,
	&dev_attr_button2.attr,
	&dev_attr_button3.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lab4_gpio);

/* ── Probe ───────────────────────────────────────────────────────────────── */

static int lab4_gpio_probe(struct platform_device *pdev)
{
	struct device    *dev = &pdev->dev;
	struct lab4_gpio *priv;
	int i, irq, ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	/* LEDs: outputs, initially off */
	for (i = 0; i < NUM_LEDS; i++) {
		priv->led[i].gpiod = devm_gpiod_get_index(dev, "led", i,
							   GPIOD_OUT_LOW);
		if (IS_ERR(priv->led[i].gpiod))
			return dev_err_probe(dev, PTR_ERR(priv->led[i].gpiod),
					     "failed to get LED %d\n", i);

		gpiod_set_consumer_name(priv->led[i].gpiod, "lkss-led");
		timer_setup(&priv->led[i].timer, led_blink_fn, 0);
	}

	/* Buttons: inputs, edge-triggered threaded IRQs */
	for (i = 0; i < NUM_BUTTONS; i++) {
		priv->btn[i] = devm_gpiod_get_index(dev, "button", i,
						     GPIOD_IN);
		if (IS_ERR(priv->btn[i]))
			return dev_err_probe(dev, PTR_ERR(priv->btn[i]),
					     "failed to get button %d\n", i);

		gpiod_set_consumer_name(priv->btn[i], "lkss-button");

		priv->btn_ctx[i].dev   = dev;
		priv->btn_ctx[i].gpiod = priv->btn[i];
		priv->btn_ctx[i].index = i + 1;  /* 1-based: BTN1..BTN4 */

		irq = gpiod_to_irq(priv->btn[i]);
		if (irq < 0)
			return dev_err_probe(dev, irq,
					     "no IRQ for button %d\n", i);

		ret = devm_request_threaded_irq(dev, irq,
						NULL,
						btn_irq_handler,
						IRQF_TRIGGER_RISING  |
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"lkss-button",
						&priv->btn_ctx[i]);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to request IRQ for button %d\n", i);
	}

	dev_info(dev, "probed: LED0(red) LED1(green) LED2(blue) + BTN1..BTN4\n");
	dev_info(dev, "LEDs:    /sys/bus/platform/devices/lkss-lab4/led{0,1,2}\n");
	dev_info(dev, "Buttons: /sys/bus/platform/devices/lkss-lab4/button{0,1,2,3}\n");
	return 0;
}

/* ── Remove ──────────────────────────────────────────────────────────────── */

static void lab4_gpio_remove(struct platform_device *pdev)
{
	struct lab4_gpio *priv = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < NUM_LEDS; i++) {
		priv->led[i].blinking = false;
		timer_delete_sync(&priv->led[i].timer);
		gpiod_set_value(priv->led[i].gpiod, 0);
	}
}

/* ── Device Tree match table ─────────────────────────────────────────────── */

static const struct of_device_id lab4_gpio_of_match[] = {
	{ .compatible = "lkss,lab4" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lab4_gpio_of_match);

/* ── Platform driver ─────────────────────────────────────────────────────── */

static struct platform_driver lab4_gpio_driver = {
	.probe  = lab4_gpio_probe,
	.remove = lab4_gpio_remove,
	.driver = {
		.name           = "lkss-lab4",
		.of_match_table = lab4_gpio_of_match,
		.dev_groups     = lab4_gpio_groups,
	},
};
module_platform_driver(lab4_gpio_driver);

MODULE_AUTHOR("LKSS Lab <lkss@nxp.com>");
MODULE_DESCRIPTION("LKSS Lab 4: GPIO LED blinking via sysfs + button IRQ handlers");
MODULE_LICENSE("GPL");
