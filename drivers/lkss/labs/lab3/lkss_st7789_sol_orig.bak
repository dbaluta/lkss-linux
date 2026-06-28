// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789_sol.c - LKSS Lab 3: ST7789 SPI display driver reference solution
 *
 * Complete implementation of every TODO in lkss_st7789.c.
 * The TODO markers are kept here, labelled "(solved)", so you can
 * diff the two files line-by-line and see exactly what each exercise adds.
 *
 * Exercise map (matches day3_bis.rst):
 *   TODO Ex-4.1/4.2   write_cmd / write_data
 *   TODO Ex-5         hw_reset
 *   TODO Ex-6         init_display
 *   TODO Ex-7.1/7.2   set_addr_win / fill
 *   TODO Ex-8         fill_rect
 *   TODO Ex-9         draw_pixel
 *   TODO Ex-10        draw_line
 *   TODO Ex-11        draw_circle
 *   TODO Ex-13        demo
 *   TODO Ex-14        miscdevice struct fields
 *   TODO Ex-15.1–4    flush / fb_write / fb_ioctl / fb_mmap
 *   TODO Ex-16        probe/remove miscdevice hooks
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define ST7789_IOC_MAGIC  'V'
#define ST7789_FLUSH      _IO(ST7789_IOC_MAGIC, 0)

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

struct st7789_priv {
	struct spi_device  *spi;
	struct gpio_desc   *dc;
	struct gpio_desc   *reset;
	u16                 width;
	u16                 height;
	/* TODO Ex-14 (solved): u8 *fbuf, size_t fbsize, struct miscdevice misc, struct mutex lock */
	u8                 *fbuf;
	size_t              fbsize;
	struct miscdevice   misc;
	struct mutex        lock;
};

/* TODO Ex-4.1 (solved): set D/C low, spi_write one command byte */
static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	gpiod_set_value(priv->dc, 0);
	return spi_write(priv->spi, &cmd, 1);
}

/* TODO Ex-4.2 (solved): set D/C high, spi_write len bytes */
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

/* TODO Ex-5 (solved): assert RST (logical 1) >= 15 ms, deassert, wait >= 120 ms */
static void st7789_hw_reset(struct st7789_priv *priv)
{
	gpiod_set_value(priv->reset, 1);
	msleep(20);
	gpiod_set_value(priv->reset, 0);
	msleep(150);
}

/*
 * TODO Ex-6 (solved): send the init sequence:
 *   SLPOUT + 600 ms, COLMOD(0x05), PORCTRL, GCTRL, VDVVRHEN, VRHS, VDVS,
 *   VCOMS, VCMOFSET, PWCTRL1, DISPON + 150 ms, INVON, MADCTL(0x00),
 *   PVGAMCTRL, NVGAMCTRL  (HSD20 IPS values, same as fb_st7789v HSD20_IPS)
 */
static int st7789_init_display(struct st7789_priv *priv)
{
	static const u8 porctrl[]   = { 0x05, 0x05, 0x00, 0x33, 0x33 };
	static const u8 vdvvrhen[]  = { 0x01, 0xFF };
	static const u8 pwctrl1[]   = { 0xA4, 0xA1 };
	static const u8 pvgamctrl[] = { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05, 0x2E,
					0x44, 0x45, 0x0F, 0x17, 0x16, 0x2B, 0x33 };
	static const u8 nvgamctrl[] = { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05, 0x2E,
					0x43, 0x45, 0x0F, 0x16, 0x16, 0x2B, 0x33 };
	int ret;

	ret = st7789_write_cmd(priv, ST7789_SLPOUT);
	if (ret) return ret;
	msleep(600);

	ret = st7789_write_cmd(priv, ST7789_COLMOD);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, ST7789_COLMOD_RGB565);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_PORCTRL);
	if (ret) return ret;
	ret = st7789_write_data(priv, porctrl, sizeof(porctrl));
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_GCTRL);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, 0x75);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_VDVVRHEN);
	if (ret) return ret;
	ret = st7789_write_data(priv, vdvvrhen, sizeof(vdvvrhen));
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_VRHS);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, 0x13);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_VDVS);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, 0x20);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_VCOMS);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, 0x22);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_VCMOFSET);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, 0x20);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_PWCTRL1);
	if (ret) return ret;
	ret = st7789_write_data(priv, pwctrl1, sizeof(pwctrl1));
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_DISPON);
	if (ret) return ret;
	msleep(150);

	ret = st7789_write_cmd(priv, ST7789_INVON);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_MADCTL);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, ST7789_MADCTL_NORMAL);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_PVGAMCTRL);
	if (ret) return ret;
	ret = st7789_write_data(priv, pvgamctrl, sizeof(pvgamctrl));
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_NVGAMCTRL);
	if (ret) return ret;
	ret = st7789_write_data(priv, nvgamctrl, sizeof(nvgamctrl));
	if (ret) return ret;

	return 0;
}

/* TODO Ex-7.1 (solved): CASET(x0,x1) + RASET(y0,y1) + RAMWR; coords as big-endian 16-bit pairs */
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

/* TODO Ex-7.2 (solved): full-panel window, kmalloc one scanline, send height rows */
static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	u8 *line;
	int x, y, ret;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret)
		return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (x = 0; x < priv->width; x++) {
		line[x * 2]     = color >> 8;
		line[x * 2 + 1] = color & 0xff;
	}

	for (y = 0; y < priv->height; y++) {
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret)
			break;
	}

	kfree(line);
	return ret;
}

/* TODO Ex-8 (solved): clamp coords to panel, set_addr_win, kmalloc row buf, send h rows */
static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	u8 *line;
	int i, row, ret = 0;

	if (x >= priv->width || y >= priv->height)
		return 0;
	if (x + w > priv->width)
		w = priv->width - x;
	if (y + h > priv->height)
		h = priv->height - y;

	ret = st7789_set_addr_win(priv, x, y, x + w - 1, y + h - 1);
	if (ret)
		return ret;

	line = kmalloc(w * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (i = 0; i < w; i++) {
		line[i * 2]     = color >> 8;
		line[i * 2 + 1] = color & 0xff;
	}

	for (row = 0; row < h; row++) {
		ret = st7789_write_data(priv, line, w * 2);
		if (ret)
			break;
	}

	kfree(line);
	return ret;
}

/* TODO Ex-9 (solved): bounds-check, 1x1 address window, send 2 pixel bytes */
static int st7789_draw_pixel(struct st7789_priv *priv,
			     u16 x, u16 y, u16 color)
{
	u8 pixel[2] = { color >> 8, color & 0xff };
	int ret;

	if (x >= priv->width || y >= priv->height)
		return 0;

	ret = st7789_set_addr_win(priv, x, y, x, y);
	if (ret)
		return ret;

	return st7789_write_data(priv, pixel, 2);
}

/* TODO Ex-10 (solved): Bresenham line algorithm, call draw_pixel each step */
static int st7789_draw_line(struct st7789_priv *priv,
			    int x0, int y0, int x1, int y1, u16 color)
{
	int dx  =  abs(x1 - x0);
	int dy  = -abs(y1 - y0);
	int sx  = (x0 < x1) ? 1 : -1;
	int sy  = (y0 < y1) ? 1 : -1;
	int err = dx + dy;
	int e2, ret;

	for (;;) {
		ret = st7789_draw_pixel(priv, (u16)x0, (u16)y0, color);
		if (ret)
			return ret;
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) { if (x0 == x1) break; err += dy; x0 += sx; }
		if (e2 <= dx) { if (y0 == y1) break; err += dx; y0 += sy; }
	}
	return 0;
}

/* TODO Ex-11 (solved): midpoint circle algorithm, 8 symmetric pixels per step */
static int st7789_draw_circle(struct st7789_priv *priv,
			      int cx, int cy, int r, u16 color)
{
	int x = 0, y = r, d = 1 - r, ret;

#define PLOT(px, py) do { \
	ret = st7789_draw_pixel(priv, (u16)(px), (u16)(py), color); \
	if (ret) return ret; \
} while (0)

	while (x <= y) {
		PLOT(cx + x, cy + y); PLOT(cx - x, cy + y);
		PLOT(cx + x, cy - y); PLOT(cx - x, cy - y);
		PLOT(cx + y, cy + x); PLOT(cx - y, cy + x);
		PLOT(cx + y, cy - x); PLOT(cx - y, cy - x);
		if (d < 0)
			d += 2 * x + 3;
		else { d += 2 * (x - y) + 5; y--; }
		x++;
	}
#undef PLOT
	return 0;
}

/* given: fill circle by drawing horizontal chords with fill_rect */
static int st7789_fill_circle(struct st7789_priv *priv,
			      int cx, int cy, int r, u16 color)
{
	int dy, dx, ret;

	for (dy = -r; dy <= r; dy++) {
		dx = (int)int_sqrt((u32)(r * r - dy * dy));
		ret = st7789_fill_rect(priv,
				       (u16)(cx - dx), (u16)(cy + dy),
				       (u16)(2 * dx + 1), 1, color);
		if (ret)
			return ret;
	}
	return 0;
}

/*
 * TODO Ex-13 (solved): draw the test pattern:
 *   1. cycle red/green/blue/white full-screen fills, 1 s each
 *   2. black background
 *   3. blue filled rectangle at top-left, 80x80
 *   4. 8 px yellow border (4x fill_rect)
 *   5. white diagonal line (0,0)→(239,239)
 *   6. white circle at (120,120) r=80
 */
static int st7789_demo(struct st7789_priv *priv)
{
	static const u16 colors[] = {
		0xF800, 0x07E0, 0x001F, 0xFFFF,  /* red, green, blue, white */
	};
	int i, ret;

	/* 1. full-screen solid color cycle, 1 s each */
	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		ret = st7789_fill(priv, colors[i]);
		if (ret) return ret;
		msleep(1000);
	}

	/* 2. black background */
	ret = st7789_fill(priv, 0x0000);
	if (ret) return ret;

	/* 3. blue rectangle top-left 80x80 */
	ret = st7789_fill_rect(priv, 0, 0, 80, 80, 0x001F);
	if (ret) return ret;

	/* 4. 8 px yellow border */
	st7789_fill_rect(priv,   0,   0, 240,   8, 0xFFE0);
	st7789_fill_rect(priv,   0, 232, 240,   8, 0xFFE0);
	st7789_fill_rect(priv,   0,   0,   8, 240, 0xFFE0);
	st7789_fill_rect(priv, 232,   0,   8, 240, 0xFFE0);

	/* 5. white diagonal */
	ret = st7789_draw_line(priv, 0, 0, 239, 239, 0xFFFF);
	if (ret) return ret;

	/* 6. white circle at center */
	ret = st7789_draw_circle(priv, 120, 120, 80, 0xFFFF);
	if (ret) return ret;

	return 0;
}

/* TODO Ex-15.1 (solved): byteswap each LE pixel to BE, flush all rows via set_addr_win + write_data */
static int st7789_flush(struct st7789_priv *priv)
{
	u8 *line;
	int ret = 0, x, y;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret)
		return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (y = 0; y < priv->height; y++) {
		const u8 *src = priv->fbuf + y * priv->width * 2;

		for (x = 0; x < priv->width; x++) {
			line[x * 2]     = src[x * 2 + 1];
			line[x * 2 + 1] = src[x * 2];
		}
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret)
			break;
	}

	kfree(line);
	return ret;
}

/* given: open always succeeds */
static int st7789_fb_open(struct inode *inode, struct file *file)
{
	return 0;
}

/* TODO Ex-15.2 (solved): copy_from_user into fbuf at *ppos, update *ppos, return bytes written */
static ssize_t st7789_fb_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct st7789_priv *priv = container_of(file->private_data,
						struct st7789_priv, misc);
	loff_t offset = *ppos;
	size_t n;

	if (offset >= (loff_t)priv->fbsize)
		return -ENOSPC;
	n = min(count, priv->fbsize - (size_t)offset);
	if (copy_from_user(priv->fbuf + offset, buf, n))
		return -EFAULT;
	*ppos += n;
	return n;
}

/* TODO Ex-15.3 (solved): accept only ST7789_FLUSH (with mutex_lock), return -ENOTTY otherwise */
static long st7789_fb_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	struct st7789_priv *priv = container_of(file->private_data,
						struct st7789_priv, misc);
	int ret;

	if (cmd != ST7789_FLUSH)
		return -ENOTTY;

	mutex_lock(&priv->lock);
	ret = st7789_flush(priv);
	mutex_unlock(&priv->lock);
	return ret;
}

/* TODO Ex-15.4 (solved): reject non-zero vm_pgoff, call remap_vmalloc_range(vma, fbuf, 0) */
static int st7789_fb_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct st7789_priv *priv = container_of(file->private_data,
						struct st7789_priv, misc);

	if (vma->vm_pgoff != 0)
		return -EINVAL;
	return remap_vmalloc_range(vma, priv->fbuf, 0);
}

static const struct file_operations st7789_fops = {
	.owner          = THIS_MODULE,
	.open           = st7789_fb_open,
	.write          = st7789_fb_write,
	.unlocked_ioctl = st7789_fb_ioctl,
	.mmap           = st7789_fb_mmap,
	.llseek         = default_llseek,
};

static int st7789_probe(struct spi_device *spi)
{
	struct st7789_priv *priv;
	int ret;

	dev_info(&spi->dev, "ST7789 probe: speed=%u Hz\n", spi->max_speed_hz);

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->spi    = spi;
	priv->width  = ST7789_WIDTH;
	priv->height = ST7789_HEIGHT;
	spi_set_drvdata(spi, priv);

	priv->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset))
		return PTR_ERR(priv->reset);

	priv->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(priv->dc))
		return PTR_ERR(priv->dc);

	/* TODO Ex-5 (solved): st7789_hw_reset(priv) */
	st7789_hw_reset(priv);

	/* TODO Ex-6, Ex-7.2 (solved): st7789_init_display(priv); st7789_fill(priv, 0x001F) */
	ret = st7789_init_display(priv);
	if (ret) {
		dev_err(&spi->dev, "display init failed: %d\n", ret);
		return ret;
	}
	ret = st7789_fill(priv, 0x001F);
	if (ret)
		return ret;

	/* TODO Ex-13 (solved): st7789_demo(priv) */
	ret = st7789_demo(priv);
	if (ret) {
		dev_err(&spi->dev, "demo failed: %d\n", ret);
		return ret;
	}

	/* TODO Ex-16 (solved): mutex_init; vmalloc_user fbuf; misc_register */
	mutex_init(&priv->lock);
	priv->fbsize = (size_t)priv->width * priv->height * 2;
	priv->fbuf   = vmalloc_user(priv->fbsize);
	if (!priv->fbuf)
		return -ENOMEM;

	priv->misc.minor = MISC_DYNAMIC_MINOR;
	priv->misc.name  = "st7789";
	priv->misc.fops  = &st7789_fops;
	ret = misc_register(&priv->misc);
	if (ret) {
		dev_err(&spi->dev, "misc_register failed: %d\n", ret);
		vfree(priv->fbuf);
		return ret;
	}

	dev_info(&spi->dev, "ST7789 ready at /dev/st7789\n");
	return 0;
}

static void st7789_remove(struct spi_device *spi)
{
	struct st7789_priv *priv = spi_get_drvdata(spi);

	/* TODO Ex-16 (solved): misc_deregister(&priv->misc); vfree(priv->fbuf) */
	misc_deregister(&priv->misc);
	vfree(priv->fbuf);

	st7789_write_cmd(priv, ST7789_DISPOFF);
	dev_info(&spi->dev, "ST7789 removed\n");
}

static const struct of_device_id st7789_of_match[] = {
	{ .compatible = "lkss,st7789" },
	{ }
};
MODULE_DEVICE_TABLE(of, st7789_of_match);

static const struct spi_device_id st7789_spi_ids[] = {
	{ "st7789", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, st7789_spi_ids);

static struct spi_driver st7789_driver = {
	.driver = {
		.name           = "lkss-st7789-sol",
		.of_match_table = st7789_of_match,
	},
	.probe    = st7789_probe,
	.remove   = st7789_remove,
	.id_table = st7789_spi_ids,
};
module_spi_driver(st7789_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("LKSS Lab 3: ST7789 SPI display driver reference solution");
MODULE_LICENSE("GPL v2");
