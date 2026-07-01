// SPDX-License-Identifier: GPL-2.0
/*
 * st7789.c LKSS Lab 3: ST7789 240x240 SPI display driver
 *
 * Exercises 4-9:  SPI primitives, reset, init, fill, rect, pixel
 * Exercise 10:    miscdevice framebuffer interface (mmap + ioctl flush)
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/ioctl.h>

/* ST7789 command opcodes */
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
#define ST7789_VCMOFSET  0xC7
#define ST7789_PWCTRL1   0xD0
#define ST7789_PVGAMCTRL 0xE0
#define ST7789_NVGAMCTRL 0xE1

#define ST7789_COLMOD_RGB565  0x55
#define ST7789_MADCTL_NORMAL  0x00

#define ST7789_WIDTH   240
#define ST7789_HEIGHT  240
#define ST7789_FB_SIZE (ST7789_WIDTH * ST7789_HEIGHT * 2)

/* 10: ioctl interface - must match the userspace definition */
#define ST7789FB_MAGIC  'F'
#define ST7789FB_FLUSH  _IO(ST7789FB_MAGIC, 0)

struct st7789_priv {
	struct spi_device *spi;
	struct gpio_desc  *dc;
	struct gpio_desc  *reset;
	u16                width;
	u16                height;

	/* 10: framebuffer miscdevice */
	void              *fb;   /* vmalloc'd RGB565 framebuffer, 240x240x2 bytes */
	struct miscdevice  mdev;
};

/* 4: Low-level SPI primitives */

/* TODO 4.1: Set DCX LOW then call spi_write() for the command byte.
 */
static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	return 0;
}

/* TODO 4.2: Set DCX HIGH then call spi_write() for len data bytes.
 */
static int st7789_write_data(struct st7789_priv *priv,
			     const u8 *buf, size_t len)
{
	return 0;
}

/* TODO 4.3: Call st7789_write_data() with &byte and length 1.
 */
static inline int st7789_write_data_byte(struct st7789_priv *priv, u8 byte)
{
	return 0;
}

/*5: Hardware reset */

/* TODO 5: Assert RESX LOW for 20 ms, deassert, wait 150 ms.
 */
static void st7789_hw_reset(struct st7789_priv *priv)
{
}

/* 6: Initialization sequence */
static int st7789_init_display(struct st7789_priv *priv)
{
	return 0;
}

/*  7: Address window and fill */

/* TODO 7.1: Send CASET + RASET + RAMWR with 4-byte big-endian coordinates.
 */
static int st7789_set_addr_win(struct st7789_priv *priv,
			       u16 x0, u16 y0, u16 x1, u16 y1)
{
	return 0;
}

/* TODO 7.2: Set full-panel address window, allocate one scanline buffer,
 * fill it with the repeated color, and send it once per row.
 */
static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	return 0;
}

/* 8: Filled rectangle */

/* TODO 8: Clamp coordinates, set address window, allocate a row buffer,
 * fill it with color, and send it h times.
 */
static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	return 0;
}

/* 9: Single pixel write */

/* TODO 9: Bounds-check (x, y), build a 2-byte big-endian pixel buffer,
 * call st7789_set_addr_win(x, y, x, y) then st7789_write_data(pixel, 2).
 */
static int st7789_draw_pixel(struct st7789_priv *priv,
			     u16 x, u16 y, u16 color)
{
	return 0;
}

/* Demo: exercises 4-9 */

static void st7789_demo(struct st7789_priv *priv)
{
	int x, y;

	/* Color cycle – visible after exercises 4–7 */
	st7789_fill(priv, 0xF800); msleep(500); /* red   */
	st7789_fill(priv, 0x07E0); msleep(500); /* green */
	st7789_fill(priv, 0x001F); msleep(500); /* blue  */
	st7789_fill(priv, 0x0000);              /* black */

	/* Red border – visible after exercise 8 */
	st7789_fill_rect(priv,   0,   0, 240,  12, 0xF800); /* top    */
	st7789_fill_rect(priv,   0, 228, 240,  12, 0xF800); /* bottom */
	st7789_fill_rect(priv,   0,   0,  12, 240, 0xF800); /* left   */
	st7789_fill_rect(priv, 228,   0,  12, 240, 0xF800); /* right  */

	/* White dot grid – visible after exercise 9 */
	for (y = 0; y < 24; y++)
		for (x = 0; x < 24; x++)
			st7789_draw_pixel(priv, x * 10, y * 10, 0xFFFF);
}

/* 10: miscdevice framebuffer interface ---- */

/*
 * Flush the vmalloc framebuffer to the ST7789 display.
 * Each row is copied into a kmalloc'd line buffer before the SPI transfer
 * because some SPI DMA engines require physically contiguous source memory.
 */
static int st7789fb_flush(struct st7789_priv *priv)
{
	u8 *line;
	int y, ret;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret)
		return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (y = 0; y < priv->height; y++) {
		memcpy(line, (u8 *)priv->fb + y * priv->width * 2, priv->width * 2);
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret)
			break;
	}

	kfree(line);
	return ret;
}

/*
 * misc open: the misc layer stores the struct miscdevice pointer in
 * filp->private_data; use container_of to recover the full st7789_priv
 * and replace private_data so mmap/ioctl can access it directly.
 */
static int st7789fb_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *mdev = filp->private_data;
	struct st7789_priv *priv = container_of(mdev, struct st7789_priv, mdev);

	filp->private_data = priv;
	return 0;
}

static int st7789fb_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct st7789_priv *priv = filp->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (size > PAGE_ALIGN(ST7789_FB_SIZE))
		return -EINVAL;

	return remap_vmalloc_range(vma, priv->fb, vma->vm_pgoff);
}

static long st7789fb_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	struct st7789_priv *priv = filp->private_data;

	switch (cmd) {
	case ST7789FB_FLUSH:
		return st7789fb_flush(priv);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations st7789fb_fops = {
	.owner          = THIS_MODULE,
	.open           = st7789fb_open,
	.mmap           = st7789fb_mmap,
	.unlocked_ioctl = st7789fb_ioctl,
};

/* SPI driver */
static int st7789_probe(struct spi_device *spi)
{
	struct st7789_priv *priv;
	int ret;

	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
		return ret;
	}

	dev_info(&spi->dev, "ST7789 probe: speed=%u Hz\n", spi->max_speed_hz);

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->spi    = spi;
	priv->width  = ST7789_WIDTH;
	priv->height = ST7789_HEIGHT;
	spi_set_drvdata(spi, priv);

	priv->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset)) {
		dev_err(&spi->dev, "failed to get reset GPIO: %ld\n",
			PTR_ERR(priv->reset));
		return PTR_ERR(priv->reset);
	}

	priv->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(priv->dc)) {
		dev_err(&spi->dev, "failed to get D/C GPIO: %ld\n",
			PTR_ERR(priv->dc));
		return PTR_ERR(priv->dc);
	}

	priv->fb = vmalloc(ST7789_FB_SIZE);
	if (!priv->fb)
		return -ENOMEM;
	memset(priv->fb, 0, ST7789_FB_SIZE);

	st7789_hw_reset(priv);

	ret = st7789_init_display(priv);
	if (ret) {
		dev_err(&spi->dev, "init failed: %d\n", ret);
		vfree(priv->fb);
		return ret;
	}

	st7789_demo(priv);

	priv->mdev.minor = MISC_DYNAMIC_MINOR;
	priv->mdev.name  = "st7789fb";
	priv->mdev.fops  = &st7789fb_fops;
	ret = misc_register(&priv->mdev);
	if (ret) {
		dev_err(&spi->dev, "misc_register failed: %d\n", ret);
		vfree(priv->fb);
		return ret;
	}

	dev_info(&spi->dev, "ST7789 ready, framebuffer at /dev/st7789fb\n");
	return 0;
}

static void st7789_remove(struct spi_device *spi)
{
	struct st7789_priv *priv = spi_get_drvdata(spi);

	misc_deregister(&priv->mdev);
	st7789_write_cmd(priv, ST7789_DISPOFF);
	vfree(priv->fb);
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
		.name           = "st7789",
		.of_match_table = st7789_of_match,
	},
	.probe    = st7789_probe,
	.remove   = st7789_remove,
	.id_table = st7789_spi_ids,
};
module_spi_driver(st7789_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("LKSS Lab 3: ST7789 SPI display driver");
MODULE_LICENSE("GPL v2");
