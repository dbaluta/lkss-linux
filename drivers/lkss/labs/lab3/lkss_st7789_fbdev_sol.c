// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789_fbdev_sol.c – LKSS Lab 3 extension:
 *   ST7789 SPI display driver with standard Linux framebuffer interface
 *
 * This variant replaces the custom miscdevice from lkss_st7789_sol.c
 * with the standard Linux framebuffer subsystem so that any fbdev-aware
 * application (LVGL, SDL1, fbv, etc.) can drive the ST7789 without
 * knowing anything about SPI or the ST7789 command set.
 *
 * Key differences from lkss_st7789_sol.c:
 *   - register_framebuffer() instead of misc_register()
 *   - Standard fbdev ioctls: FBIOGET_VSCREENINFO, FBIOGET_FSCREENINFO
 *   - vmalloc_user framebuffer exposed via standard fb mmap
 *   - A kernel thread flushes the framebuffer to SPI at ~30 fps
 *
 * Userspace usage (any fbdev app, e.g. LVGL):
 *   fd = open("/dev/fb0", O_RDWR);
 *   ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);   // 240x240, 16bpp
 *   fb = mmap(NULL, 240*240*2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
 *   // Write RGB565 pixels directly — display updates at ~30 fps automatically
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/kthread.h>

/* ST7789 command opcodes */
#define ST7789_SWRESET   0x01
#define ST7789_SLPOUT    0x11
#define ST7789_NORON     0x13
#define ST7789_INVON     0x21
#define ST7789_DISPOFF   0x28
#define ST7789_DISPON    0x29
#define ST7789_CASET     0x2A
#define ST7789_RASET     0x2B
#define ST7789_RAMWR     0x2C
#define ST7789_MADCTL    0x36
#define ST7789_COLMOD    0x3A
#define ST7789_PORCTRL   0xB2
#define ST7789_GCTRL     0xB7
#define ST7789_VCOMS     0xBB
#define ST7789_VDVVRHEN  0xC2
#define ST7789_VRHS      0xC3
#define ST7789_VDVS      0xC4
#define ST7789_VCMOFSET  0xC5
#define ST7789_PWCTRL1   0xD0
#define ST7789_PVGAMCTRL 0xE0
#define ST7789_NVGAMCTRL 0xE1

#define ST7789_COLMOD_RGB565  0x05
#define ST7789_MADCTL_NORMAL  0x00

#define ST7789_WIDTH   240
#define ST7789_HEIGHT  240
#define ST7789_FBSIZE  (ST7789_WIDTH * ST7789_HEIGHT * 2)

/* Flush interval: 33 ms ≈ 30 fps */
#define FLUSH_INTERVAL_MS  33

struct st7789_priv {
	struct spi_device    *spi;
	struct gpio_desc     *dc;
	struct gpio_desc     *reset;
	u16                   width;
	u16                   height;
	struct fb_info       *fbinfo;    /* registered framebuffer             */
	struct mutex          lock;      /* protects SPI access from kthread   */
	struct task_struct   *flush_tsk; /* kthread: periodically flushes FB   */
};

/* ── Low-level SPI primitives ─────────────────────────────────────────── */

static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	gpiod_set_value(priv->dc, 0);
	return spi_write(priv->spi, &cmd, 1);
}

static int st7789_write_data(struct st7789_priv *priv,
			     const u8 *buf, size_t len)
{
	gpiod_set_value(priv->dc, 1);
	return spi_write(priv->spi, buf, len);
}

static inline int st7789_write_data_byte(struct st7789_priv *priv, u8 byte)
{
	return st7789_write_data(priv, &byte, 1);
}

/* ── Hardware reset ───────────────────────────────────────────────────── */

static void st7789_hw_reset(struct st7789_priv *priv)
{
	gpiod_set_value(priv->reset, 1);
	msleep(20);
	gpiod_set_value(priv->reset, 0);
	msleep(150);
}

/* ── Initialization sequence (HSD20 IPS profile) ─────────────────────── */

static int st7789_init_display(struct st7789_priv *priv)
{
	static const u8 porctrl[]   = { 0x05, 0x05, 0x00, 0x33, 0x33 };
	static const u8 vdvvrhen[]  = { 0x01, 0xFF };
	static const u8 pwctrl1[]   = { 0xA4, 0xA1 };
	static const u8 pvgamctrl[] = { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05,
					0x2E, 0x44, 0x45, 0x0F, 0x17, 0x16,
					0x2B, 0x33 };
	static const u8 nvgamctrl[] = { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05,
					0x2E, 0x43, 0x45, 0x0F, 0x16, 0x16,
					0x2B, 0x33 };
	int ret;

#define CMD(c)      do { ret = st7789_write_cmd(priv, c); if (ret) return ret; } while (0)
#define DATA(d)     do { ret = st7789_write_data_byte(priv, d); if (ret) return ret; } while (0)
#define BUF(b)      do { ret = st7789_write_data(priv, b, sizeof(b)); if (ret) return ret; } while (0)

	CMD(ST7789_SLPOUT);   msleep(600);
	CMD(ST7789_COLMOD);   DATA(ST7789_COLMOD_RGB565);
	CMD(ST7789_PORCTRL);  BUF(porctrl);
	CMD(ST7789_GCTRL);    DATA(0x75);
	CMD(ST7789_VDVVRHEN); BUF(vdvvrhen);
	CMD(ST7789_VRHS);     DATA(0x13);
	CMD(ST7789_VDVS);     DATA(0x20);
	CMD(ST7789_VCOMS);    DATA(0x22);
	CMD(ST7789_VCMOFSET); DATA(0x20);
	CMD(ST7789_PWCTRL1);  BUF(pwctrl1);
	CMD(ST7789_DISPON);   msleep(150);
	CMD(ST7789_INVON);
	CMD(ST7789_MADCTL);   DATA(ST7789_MADCTL_NORMAL);
	CMD(ST7789_PVGAMCTRL); BUF(pvgamctrl);
	CMD(ST7789_NVGAMCTRL); BUF(nvgamctrl);

#undef CMD
#undef DATA
#undef BUF
	return 0;
}

/* ── Drawing primitives (used for startup splash only) ───────────────── */

static int st7789_set_addr_win(struct st7789_priv *priv,
			       u16 x0, u16 y0, u16 x1, u16 y1)
{
	u8 col[4] = { x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff };
	u8 row[4] = { y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff };
	int ret;

	ret = st7789_write_cmd(priv, ST7789_CASET);
	if (ret) return ret;
	ret = st7789_write_data(priv, col, 4);
	if (ret) return ret;
	ret = st7789_write_cmd(priv, ST7789_RASET);
	if (ret) return ret;
	ret = st7789_write_data(priv, row, 4);
	if (ret) return ret;
	return st7789_write_cmd(priv, ST7789_RAMWR);
}

static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	u8 *line;
	int x, y, ret;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret) return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line) return -ENOMEM;

	for (x = 0; x < priv->width; x++) {
		line[x * 2]     = color >> 8;
		line[x * 2 + 1] = color & 0xff;
	}
	for (y = 0; y < priv->height; y++) {
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret) break;
	}
	kfree(line);
	return ret;
}

/* ── Framebuffer → SPI flush ──────────────────────────────────────────── */

/*
 * Reads the vmalloc framebuffer (little-endian RGB565, as written by
 * userspace) and sends it to the ST7789 over SPI with per-pixel byteswap
 * (the display expects big-endian / MSB-first).
 *
 * Called from the flush kthread with priv->lock held.
 */
static int st7789_fb_flush(struct st7789_priv *priv)
{
	u8 *line;
	u8 *screen = (u8 *)priv->fbinfo->screen_base;
	int ret = 0, x, y;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret) return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line) return -ENOMEM;

	for (y = 0; y < priv->height; y++) {
		const u8 *src = screen + y * priv->width * 2;
		/* Swap LE bytes to big-endian for ST7789 SPI protocol */
		for (x = 0; x < priv->width; x++) {
			line[x * 2]     = src[x * 2 + 1];
			line[x * 2 + 1] = src[x * 2];
		}
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret) break;
	}

	kfree(line);
	return ret;
}

/* ── Flush kernel thread ──────────────────────────────────────────────── */

static int st7789_flush_thread(void *data)
{
	struct st7789_priv *priv = data;

	while (!kthread_should_stop()) {
		mutex_lock(&priv->lock);
		st7789_fb_flush(priv);
		mutex_unlock(&priv->lock);
		msleep(FLUSH_INTERVAL_MS);
	}
	return 0;
}

/* ── Framebuffer mmap ─────────────────────────────────────────────────── */

static int st7789_fbdev_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	/*
	 * Map the vmalloc_user framebuffer into userspace.
	 * remap_vmalloc_range() requires VM_USERMAP (set by vmalloc_user).
	 */
	return remap_vmalloc_range(vma, info->screen_base, vma->vm_pgoff);
}

/* Minimal stubs — LVGL uses mmap and never calls these */
static void st7789_fb_fillrect(struct fb_info *i, const struct fb_fillrect *r)  {}
static void st7789_fb_copyarea(struct fb_info *i, const struct fb_copyarea *a)  {}
static void st7789_fb_imageblit(struct fb_info *i, const struct fb_image *img)  {}

static const struct fb_ops st7789_fbdev_ops = {
	.owner        = THIS_MODULE,
	.fb_mmap      = st7789_fbdev_mmap,
	.fb_fillrect  = st7789_fb_fillrect,
	.fb_copyarea  = st7789_fb_copyarea,
	.fb_imageblit = st7789_fb_imageblit,
};

/* ── Probe ────────────────────────────────────────────────────────────── */

static int st7789_probe(struct spi_device *spi)
{
	struct st7789_priv *priv;
	struct fb_info *info;
	int ret;

	dev_info(&spi->dev, "ST7789-fbdev probe: speed=%u Hz\n",
		 spi->max_speed_hz);

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->spi    = spi;
	priv->width  = ST7789_WIDTH;
	priv->height = ST7789_HEIGHT;
	mutex_init(&priv->lock);
	spi_set_drvdata(spi, priv);

	priv->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset))
		return PTR_ERR(priv->reset);

	priv->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(priv->dc))
		return PTR_ERR(priv->dc);

	st7789_hw_reset(priv);

	ret = st7789_init_display(priv);
	if (ret) {
		dev_err(&spi->dev, "display init failed: %d\n", ret);
		return ret;
	}

	/* Splash screen: black background */
	ret = st7789_fill(priv, 0x0000);
	if (ret)
		return ret;

	/* Allocate and register the framebuffer */
	info = framebuffer_alloc(0, &spi->dev);
	if (!info)
		return -ENOMEM;

	info->screen_base = vmalloc_user(ST7789_FBSIZE);
	if (!info->screen_base) {
		framebuffer_release(info);
		return -ENOMEM;
	}
	memset(info->screen_base, 0, ST7789_FBSIZE);  /* black initial content */

	info->fbops = &st7789_fbdev_ops;
	info->par   = priv;

	strscpy(info->fix.id, "st7789", sizeof(info->fix.id));
	info->fix.type       = FB_TYPE_PACKED_PIXELS;
	info->fix.visual     = FB_VISUAL_TRUECOLOR;
	info->fix.line_length = ST7789_WIDTH * 2;  /* 480 bytes/row */
	info->fix.smem_len   = ST7789_FBSIZE;
	info->fix.smem_start = 0;  /* vmalloc, no physical address */

	info->var.xres         = ST7789_WIDTH;
	info->var.yres         = ST7789_HEIGHT;
	info->var.xres_virtual = ST7789_WIDTH;
	info->var.yres_virtual = ST7789_HEIGHT;
	info->var.bits_per_pixel = 16;
	/* RGB565 in little-endian: R[15:11] G[10:5] B[4:0] */
	info->var.red.offset   = 11; info->var.red.length   = 5;
	info->var.green.offset = 5;  info->var.green.length = 6;
	info->var.blue.offset  = 0;  info->var.blue.length  = 5;
	info->var.transp.length = 0;
	info->var.activate = FB_ACTIVATE_NOW;

	priv->fbinfo = info;

	ret = register_framebuffer(info);
	if (ret) {
		dev_err(&spi->dev, "register_framebuffer failed: %d\n", ret);
		vfree(info->screen_base);
		framebuffer_release(info);
		return ret;
	}

	/* Start the flush kthread */
	priv->flush_tsk = kthread_run(st7789_flush_thread, priv,
				      "st7789-flush");
	if (IS_ERR(priv->flush_tsk)) {
		ret = PTR_ERR(priv->flush_tsk);
		dev_err(&spi->dev, "kthread_run failed: %d\n", ret);
		unregister_framebuffer(info);
		vfree(info->screen_base);
		framebuffer_release(info);
		return ret;
	}

	dev_info(&spi->dev, "ST7789 ready at /dev/fb%d\n",
		 info->node);
	return 0;
}

static void st7789_remove(struct spi_device *spi)
{
	struct st7789_priv *priv = spi_get_drvdata(spi);
	struct fb_info *info = priv->fbinfo;

	kthread_stop(priv->flush_tsk);

	unregister_framebuffer(info);
	vfree(info->screen_base);
	framebuffer_release(info);

	st7789_write_cmd(priv, ST7789_DISPOFF);
	dev_info(&spi->dev, "ST7789 fbdev removed\n");
}

static const struct of_device_id st7789_fbdev_of_match[] = {
	{ .compatible = "lkss,st7789" },
	{ }
};
MODULE_DEVICE_TABLE(of, st7789_fbdev_of_match);

static const struct spi_device_id st7789_fbdev_spi_ids[] = {
	{ "st7789", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, st7789_fbdev_spi_ids);

static struct spi_driver st7789_fbdev_driver = {
	.driver = {
		.name           = "lkss-st7789-fbdev",
		.of_match_table = st7789_fbdev_of_match,
	},
	.probe    = st7789_probe,
	.remove   = st7789_remove,
	.id_table = st7789_fbdev_spi_ids,
};
module_spi_driver(st7789_fbdev_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("LKSS Lab 3 extension: ST7789 with standard fbdev interface");
MODULE_LICENSE("GPL v2");
