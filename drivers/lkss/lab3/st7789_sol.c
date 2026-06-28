// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789_sol.c – LKSS Lab 3: ST7789 240×240 SPI display driver (solution)
 *
 * Complete reference implementation.  Not distributed to students.
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>

/* ── ST7789 command opcodes ─────────────────────────────────────────────── */
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

/* ── Driver private state ─────────────────────────────────────────────── */

struct st7789_priv {
	struct spi_device *spi;
	struct gpio_desc  *dc;
	struct gpio_desc  *reset;
	u16                width;
	u16                height;
};

/* ── TODO 4.1 solution ────────────────────────────────────────────────── */

static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	gpiod_set_value(priv->dc, 0);
	return spi_write(priv->spi, &cmd, 1);
}

/* ── TODO 4.2 solution ────────────────────────────────────────────────── */

static int st7789_write_data(struct st7789_priv *priv,
			     const u8 *buf, size_t len)
{
	gpiod_set_value(priv->dc, 1);
	return spi_write(priv->spi, buf, len);
}

/* ── TODO 4.3 solution ────────────────────────────────────────────────── */

static inline int st7789_write_data_byte(struct st7789_priv *priv, u8 byte)
{
	return st7789_write_data(priv, &byte, 1);
}

/* ── TODO 5 solution ─────────────────────────────────────────────────── */

static void st7789_hw_reset(struct st7789_priv *priv)
{
	gpiod_set_value(priv->reset, 1);
	msleep(20);
	gpiod_set_value(priv->reset, 0);
	msleep(150);
}

/* ── TODO 6 solution: full HSD20_IPS initialization ─────────────────── */

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

/* ── TODO 7.1 solution ────────────────────────────────────────────────── */

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

/* ── TODO 7.2 solution ────────────────────────────────────────────────── */

static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	u8 hi = color >> 8, lo = color & 0xff;
	u8 *line;
	int ret = 0, x, y;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret) return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line) return -ENOMEM;

	for (x = 0; x < priv->width; x++) {
		line[x * 2]     = hi;
		line[x * 2 + 1] = lo;
	}
	for (y = 0; y < priv->height; y++) {
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret) break;
	}
	kfree(line);
	return ret;
}

/* ── TODO 8 solution ─────────────────────────────────────────────────── */

static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	u8 hi = color >> 8, lo = color & 0xff;
	u8 *line;
	int ret = 0;
	u16 i, row;

	if (x >= priv->width || y >= priv->height) return 0;
	if (x + w > priv->width)  w = priv->width  - x;
	if (y + h > priv->height) h = priv->height - y;

	ret = st7789_set_addr_win(priv, x, y, x + w - 1, y + h - 1);
	if (ret) return ret;

	line = kmalloc(w * 2, GFP_KERNEL);
	if (!line) return -ENOMEM;

	for (i = 0; i < w; i++) {
		line[i * 2]     = hi;
		line[i * 2 + 1] = lo;
	}
	for (row = 0; row < h; row++) {
		ret = st7789_write_data(priv, line, w * 2);
		if (ret) break;
	}
	kfree(line);
	return ret;
}

/* ── TODO 9 solution ─────────────────────────────────────────────────── */

static int st7789_draw_pixel(struct st7789_priv *priv,
			     u16 x, u16 y, u16 color)
{
	u8 pixel[2] = { color >> 8, color & 0xff };
	int ret;

	if (x >= priv->width || y >= priv->height)
		return 0;
	ret = st7789_set_addr_win(priv, x, y, x, y);
	if (ret) return ret;
	return st7789_write_data(priv, pixel, 2);
}

/* ── Demo ─────────────────────────────────────────────────────────────── */

static void st7789_demo(struct st7789_priv *priv)
{
	int x, y;

	st7789_fill(priv, 0xF800); msleep(500);
	st7789_fill(priv, 0x07E0); msleep(500);
	st7789_fill(priv, 0x001F); msleep(500);
	st7789_fill(priv, 0x0000);

	st7789_fill_rect(priv,   0,   0, 240,  12, 0xF800);
	st7789_fill_rect(priv,   0, 228, 240,  12, 0xF800);
	st7789_fill_rect(priv,   0,   0,  12, 240, 0xF800);
	st7789_fill_rect(priv, 228,   0,  12, 240, 0xF800);

	for (y = 0; y < 24; y++)
		for (x = 0; x < 24; x++)
			st7789_draw_pixel(priv, x * 10, y * 10, 0xFFFF);
}

/* ── SPI driver boilerplate ───────────────────────────────────────────── */

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

	st7789_hw_reset(priv);

	ret = st7789_init_display(priv);
	if (ret) {
		dev_err(&spi->dev, "init failed: %d\n", ret);
		return ret;
	}

	st7789_demo(priv);

	dev_info(&spi->dev, "ST7789 ready\n");
	return 0;
}

static void st7789_remove(struct spi_device *spi)
{
	struct st7789_priv *priv = spi_get_drvdata(spi);

	st7789_write_cmd(priv, ST7789_DISPOFF);
	dev_info(&spi->dev, "ST7789 removed\n");
}

static const struct of_device_id st7789_of_match[] = {
	{ .compatible = "lkss,st7789" },
	{ }
};
MODULE_DEVICE_TABLE(of, st7789_of_match);

static const struct spi_device_id st7789_spi_ids[] = {
	{ "st7789_sol", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, st7789_spi_ids);

static struct spi_driver st7789_driver = {
	.driver = {
		.name           = "st7789-sol",
		.of_match_table = st7789_of_match,
	},
	.probe    = st7789_probe,
	.remove   = st7789_remove,
	.id_table = st7789_spi_ids,
};
module_spi_driver(st7789_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("LKSS Lab 3: ST7789 SPI display driver (solution)");
MODULE_LICENSE("GPL v2");
