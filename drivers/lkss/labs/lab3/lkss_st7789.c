// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789.c - LKSS Lab 3: ST7789 SPI display driver skeleton
 *
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
	/* TODO Ex-14: add u8 *fbuf, size_t fbsize, struct miscdevice misc, struct mutex lock */
};

/* TODO Ex-4.1: set D/C pin low, then spi_write one command byte */
static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{

	return -EOPNOTSUPP;
}

/* TODO Ex-4.2: set D/C pin high, then spi_write len bytes */
static int st7789_write_data(struct st7789_priv *priv,
			     const u8 *buf, size_t len)
{
	return -EOPNOTSUPP;
}

static inline int st7789_write_data_byte(struct st7789_priv *priv, u8 byte)
{
	return st7789_write_data(priv, &byte, 1);
}

/* TODO Ex-5: assert RST (logical 1) >= 15 ms, deassert, wait >= 120 ms */
static void st7789_hw_reset(struct st7789_priv *priv)
{
}

/*
 * TODO Ex-6: send the init sequence:
 *   SLPOUT + 600 ms, COLMOD(0x05), PORCTRL, GCTRL, VDVVRHEN, VRHS, VDVS,
 *   VCOMS, VCMOFSET, PWCTRL1, DISPON + 150 ms, INVON, MADCTL(0x00),
 *   PVGAMCTRL, NVGAMCTRL  (HSD20 IPS values, same as fb_st7789v HSD20_IPS)
 */
static int st7789_init_display(struct st7789_priv *priv)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-7.1: CASET(x0,x1) + RASET(y0,y1) + RAMWR; coords as big-endian 16-bit pairs */
static int st7789_set_addr_win(struct st7789_priv *priv,
			       u16 x0, u16 y0, u16 x1, u16 y1)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-7.2: full-panel window, kmalloc one scanline, send height rows */
static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-8: clamp coords to panel, set_addr_win, kmalloc row buf, send h rows */
static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-9: bounds-check, 1x1 address window, send 2 pixel bytes */
static int st7789_draw_pixel(struct st7789_priv *priv,
			     u16 x, u16 y, u16 color)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-10: Bresenham line algorithm, call draw_pixel each step */
static int st7789_draw_line(struct st7789_priv *priv,
			    int x0, int y0, int x1, int y1, u16 color)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-11: midpoint circle algorithm, 8 symmetric pixels per step */
static int st7789_draw_circle(struct st7789_priv *priv,
			      int cx, int cy, int r, u16 color)
{
	return -EOPNOTSUPP;
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
 * TODO Ex-13: draw the test pattern:
 *   1. cycle red/green/blue/white full-screen fills, 1 s each
 *   2. black background
 *   3. blue filled rectangle at top-left, 80x80
 *   4. 8 px yellow border (4x fill_rect)
 *   5. white diagonal line (0,0)→(239,239)
 *   6. white circle at (120,120) r=80
 */
static int st7789_demo(struct st7789_priv *priv)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-15.1: byteswap each LE pixel to BE, flush all rows via set_addr_win + write_data */
static int st7789_flush(struct st7789_priv *priv)
{
	return -EOPNOTSUPP;
}

/* given: open always succeeds */
static int st7789_fb_open(struct inode *inode, struct file *file)
{
	return 0;
}

/* TODO Ex-15.2: copy_from_user into fbuf at *ppos, update *ppos, return bytes written */
static ssize_t st7789_fb_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-15.3: accept only ST7789_FLUSH (with mutex_lock), return -ENOTTY otherwise */
static long st7789_fb_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	return -EOPNOTSUPP;
}

/* TODO Ex-15.4: reject non-zero vm_pgoff, call remap_vmalloc_range(vma, fbuf, 0) */
static int st7789_fb_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EOPNOTSUPP;
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

	/* TODO Ex-5: st7789_hw_reset(priv) */

	/* TODO Ex-6, Ex-7.2: st7789_init_display(priv); st7789_fill(priv, 0x001F) */

	/* TODO Ex-13: st7789_demo(priv) */

	/* TODO Ex-16: mutex_init; vmalloc_user fbuf; misc_register */

	dev_info(&spi->dev, "ST7789 ready\n");
	return 0;
}

static void st7789_remove(struct spi_device *spi)
{
	struct st7789_priv *priv = spi_get_drvdata(spi);

	/* TODO Ex-16: misc_deregister(&priv->misc); vfree(priv->fbuf) */

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
		.name           = "lkss-st7789",
		.of_match_table = st7789_of_match,
	},
	.probe    = st7789_probe,
	.remove   = st7789_remove,
	.id_table = st7789_spi_ids,
};
module_spi_driver(st7789_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("LKSS Lab 3: ST7789 SPI display driver skeleton");
MODULE_LICENSE("GPL v2");
