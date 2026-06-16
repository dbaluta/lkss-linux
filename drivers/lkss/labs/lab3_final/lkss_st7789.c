// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789.c  –  LKSS Lab 3 SOLUTION
 *
 * Complete from-scratch SPI driver for the ST7789 240×240 TFT display.
 * Implements: hw_reset, init, fill, fill_rect, draw_pixel, draw_line,
 *             draw_circle, fill_circle, and a colourful demo pattern.
 *
 * Hardware wiring (LPSPI5 on i.MX93 FRDM expansion header):
 *   GPIO_IO21  →  LPSPI5_SCK   (serial clock)
 *   GPIO_IO20  →  LPSPI5_SOUT  (MOSI)
 *   GPIO_IO18  →  LPSPI5_PCS0  (hardware CS, toggled by SPI core;
 *                                display CS is tied to GND on the module)
 *   GPIO_IO19  →  RST           (active-low hardware reset, plain GPIO)
 *   GPIO_IO13  →  D/C           (data/command select, active-high = data)
 *
 * Device tree compatible: "lkss,lkss-st7789"
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>

/* =========================================================================
 * ST7789 command opcodes  (ST7789VW datasheet, chapter 9)
 * ========================================================================= */
#define ST7789_NOP      0x00
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A

/* COLMOD parameter: RGB565 (16 bpp) */
#define ST7789_COLMOD_RGB565  0x55
/* MADCTL parameter: normal scan direction, RGB colour order */
#define ST7789_MADCTL_NORMAL  0x00

/* Panel dimensions for the 240×240 module */
#define ST7789_WIDTH   240
#define ST7789_HEIGHT  240

/* Handy RGB565 colour constants */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_ORANGE  0xFD20

/* =========================================================================
 * Driver private state
 * ========================================================================= */
struct st7789_priv {
	struct spi_device *spi;
	struct gpio_desc  *dc;
	struct gpio_desc  *reset;
	u16                width;
	u16                height;
};

/* =========================================================================
 * Low-level SPI primitives
 * ========================================================================= */

/*
 * st7789_write_cmd - send a single command byte
 *
 * Sets D/C LOW (command mode) and clocks the byte out over SPI.
 * The SPI core asserts and de-asserts CS around the spi_write() call.
 */
static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	gpiod_set_value(priv->dc, 0);
	return spi_write(priv->spi, &cmd, 1);
}

/*
 * st7789_write_data - send one or more data bytes
 *
 * Sets D/C HIGH (data mode) and clocks len bytes out over SPI.
 * Called after a command to send its parameters, or after RAMWR to
 * stream pixel data.
 */
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

/* =========================================================================
 * Hardware reset
 * ========================================================================= */

/*
 * st7789_hw_reset - pulse the RESX line to reset the controller
 *
 * ST7789VW datasheet §8.16:
 *   - Drive RESX LOW for ≥ 15 ms
 *   - Drive RESX HIGH and wait ≥ 120 ms before the first command
 *
 * gpiod_set_value() uses logical values: 1 = assert (active).
 * Because the GPIO is declared GPIO_ACTIVE_LOW in the DT, logical 1
 * drives the physical pin LOW.
 */
static void st7789_hw_reset(struct st7789_priv *priv)
{
	gpiod_set_value(priv->reset, 1);  /* assert  → pin LOW  */
	msleep(15);
	gpiod_set_value(priv->reset, 0);  /* deassert → pin HIGH */
	msleep(120);
}

/* =========================================================================
 * Initialization sequence
 * ========================================================================= */

/*
 * st7789_init_display - send the power-on initialization command sequence
 *
 * After hw_reset the controller is in sleep mode. This sequence wakes
 * it up, sets the pixel format to RGB565, and turns on the display output.
 */
static int st7789_init_display(struct st7789_priv *priv)
{
	int ret;

	/* Software reset: all registers → factory defaults.  Wait ≥ 150 ms. */
	ret = st7789_write_cmd(priv, ST7789_SWRESET);
	if (ret)
		return ret;
	msleep(150);

	/* Exit sleep mode.  DC/DC converter and oscillator start.
	 * Must wait ≥ 500 ms before sending DISPON.               */
	ret = st7789_write_cmd(priv, ST7789_SLPOUT);
	if (ret)
		return ret;
	msleep(500);

	/* Pixel format → RGB565 (16 bpp).
	 * Parameter 0x55: DPI=101 (RGB565), DBI=101 (RGB565).    */
	ret = st7789_write_cmd(priv, ST7789_COLMOD);
	if (ret)
		return ret;
	ret = st7789_write_data_byte(priv, ST7789_COLMOD_RGB565);
	if (ret)
		return ret;

	/* Memory access control: normal scan direction, RGB colour order. */
	ret = st7789_write_cmd(priv, ST7789_MADCTL);
	if (ret)
		return ret;
	ret = st7789_write_data_byte(priv, ST7789_MADCTL_NORMAL);
	if (ret)
		return ret;

	/* Enable display inversion.  Most ST7789 modules are manufactured
	 * with the inversion bit set by default; INVON produces correct
	 * (non-inverted) colours on such panels.  Swap for INVOFF if
	 * colours appear wrong.                                           */
	ret = st7789_write_cmd(priv, ST7789_INVON);
	if (ret)
		return ret;

	/* Normal display mode (no partial mode). */
	ret = st7789_write_cmd(priv, ST7789_NORON);
	if (ret)
		return ret;

	/* Turn on the display output.  Wait ≥ 100 ms. */
	ret = st7789_write_cmd(priv, ST7789_DISPON);
	if (ret)
		return ret;
	msleep(100);

	return 0;
}

/* =========================================================================
 * Address window and pixel fill
 * ========================================================================= */

/*
 * st7789_set_addr_win - set the active drawing rectangle
 * @x0,y0: top-left corner  (inclusive, 0-based)
 * @x1,y1: bottom-right corner (inclusive)
 *
 * After this call, pixel data sent with write_data() fills the rectangle
 * [x0..x1] × [y0..y1] in row-major order (left→right, top→bottom).
 *
 * CASET/RASET encode start and end as big-endian 16-bit pairs.
 */
static int st7789_set_addr_win(struct st7789_priv *priv,
			       u16 x0, u16 y0, u16 x1, u16 y1)
{
	u8 col[4] = { x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff };
	u8 row[4] = { y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff };
	int ret;

	ret = st7789_write_cmd(priv, ST7789_CASET);
	if (ret)
		return ret;
	ret = st7789_write_data(priv, col, 4);
	if (ret)
		return ret;

	ret = st7789_write_cmd(priv, ST7789_RASET);
	if (ret)
		return ret;
	ret = st7789_write_data(priv, row, 4);
	if (ret)
		return ret;

	/* RAMWR: following write_data() calls are pixel data. */
	return st7789_write_cmd(priv, ST7789_RAMWR);
}

/*
 * st7789_fill - fill the entire display with a single RGB565 colour
 *
 * Sends one full scanline buffer at a time to keep the per-transaction
 * buffer size reasonable (480 bytes per row vs. 115 200 for the full frame).
 */
static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	u8 color_hi = color >> 8;
	u8 color_lo = color & 0xff;
	u8 *line;
	int ret = 0, x, y;

	ret = st7789_set_addr_win(priv, 0, 0,
				  priv->width - 1, priv->height - 1);
	if (ret)
		return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (x = 0; x < priv->width; x++) {
		line[x * 2]     = color_hi;
		line[x * 2 + 1] = color_lo;
	}

	for (y = 0; y < priv->height; y++) {
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret)
			break;
	}

	kfree(line);
	return ret;
}

/* =========================================================================
 * Filled rectangle
 * ========================================================================= */

/*
 * st7789_fill_rect - draw a filled rectangle
 * @x,y: top-left corner
 * @w,h: width and height in pixels
 * @color: RGB565 fill colour
 *
 * Coordinates outside the panel are silently clamped.
 */
static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	u8 color_hi = color >> 8;
	u8 color_lo = color & 0xff;
	u8 *line;
	u16 i, row;
	int ret = 0;

	if (x >= priv->width || y >= priv->height)
		return 0;
	if (x + w > priv->width)
		w = priv->width - x;
	if (y + h > priv->height)
		h = priv->height - y;
	if (!w || !h)
		return 0;

	ret = st7789_set_addr_win(priv, x, y, x + w - 1, y + h - 1);
	if (ret)
		return ret;

	line = kmalloc(w * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (i = 0; i < w; i++) {
		line[i * 2]     = color_hi;
		line[i * 2 + 1] = color_lo;
	}

	for (row = 0; row < h; row++) {
		ret = st7789_write_data(priv, line, w * 2);
		if (ret)
			break;
	}

	kfree(line);
	return ret;
}

/* =========================================================================
 * Single-pixel draw
 * ========================================================================= */

/*
 * st7789_draw_pixel - write a single pixel
 *
 * Sets a 1×1 address window then sends 2 bytes of RGB565 colour data.
 * Coordinates out of range are silently ignored.
 */
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

/* =========================================================================
 * Bresenham line drawing
 * ========================================================================= */

/*
 * st7789_draw_line - draw a straight line between two points
 *
 * Uses Bresenham's integer algorithm (IBM Systems Journal, 1965).
 * The error accumulator 'err' tracks the deviation from an ideal line;
 * when it exceeds a threshold we step in the minor axis direction.
 */
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
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
	return 0;
}

/* =========================================================================
 * Midpoint circle algorithm (outline)
 * ========================================================================= */

/*
 * st7789_draw_circle - draw the outline of a circle
 *
 * Uses the midpoint circle algorithm which exploits 8-fold symmetry
 * to draw 8 pixels per iteration using only integer arithmetic.
 * The decision parameter 'd' tracks whether the circle boundary has
 * moved inside or outside the current pixel position.
 */
static int st7789_draw_circle(struct st7789_priv *priv,
			      int cx, int cy, int r, u16 color)
{
	int x = 0, y = r, d = 1 - r, ret;

#define PLOT(px, py) do {						\
	ret = st7789_draw_pixel(priv, (u16)(px), (u16)(py), color);	\
	if (ret) return ret;						\
} while (0)

	while (x <= y) {
		PLOT(cx + x, cy + y);
		PLOT(cx - x, cy + y);
		PLOT(cx + x, cy - y);
		PLOT(cx - x, cy - y);
		PLOT(cx + y, cy + x);
		PLOT(cx - y, cy + x);
		PLOT(cx + y, cy - x);
		PLOT(cx - y, cy - x);

		if (d < 0)
			d += 2 * x + 3;
		else { d += 2 * (x - y) + 5; y--; }
		x++;
	}
#undef PLOT
	return 0;
}

/* =========================================================================
 * Filled circle
 * ========================================================================= */

/*
 * st7789_fill_circle - draw a solid filled circle
 *
 * For each row dy within [-r, r] compute the horizontal chord half-width
 * dx = sqrt(r² - dy²) and fill a horizontal strip of width 2*dx+1.
 */
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

/* =========================================================================
 * Demo pattern – the "wow" moment
 * ========================================================================= */

/*
 * st7789_demo - draw an impressive test pattern on the display
 *
 * Layout (240×240):
 *   Rows   0–79:  rainbow stripes (8 colours × 10 px)
 *   Rows  80–239: drawing canvas
 *     - White 2-pixel border around the full panel
 *     - 5 concentric circles centred at (120, 160), radii 70→10
 *     - 2 diagonal white lines crossing at (120, 160)
 *     - Filled cyan circle top-left of canvas (60, 130), r=25
 *     - Filled magenta circle top-right of canvas (180, 130), r=25
 */
static int st7789_demo(struct st7789_priv *priv)
{
	static const u16 stripe_colors[8] = {
		COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN,
		COLOR_CYAN, COLOR_BLUE,  COLOR_MAGENTA, COLOR_WHITE,
	};
	static const u16 circle_colors[5] = {
		COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_WHITE,
	};
	int i, ret;

	/* 1 – Black background */
	ret = st7789_fill(priv, COLOR_BLACK);
	if (ret)
		return ret;

	/* 2 – Rainbow stripes across the top 80 pixels (8 × 10 px) */
	for (i = 0; i < 8; i++) {
		ret = st7789_fill_rect(priv, 0, i * 10, 240, 10,
				       stripe_colors[i]);
		if (ret)
			return ret;
	}

	/* 3 – White 2-pixel border around the entire panel */
	st7789_fill_rect(priv,   0,   0, 240,   2, COLOR_WHITE);
	st7789_fill_rect(priv,   0, 238, 240,   2, COLOR_WHITE);
	st7789_fill_rect(priv,   0,   0,   2, 240, COLOR_WHITE);
	st7789_fill_rect(priv, 238,   0,   2, 240, COLOR_WHITE);

	/* 4 – Five concentric circle outlines centred at (120, 160) */
	for (i = 0; i < 5; i++) {
		ret = st7789_draw_circle(priv, 120, 160, 70 - i * 15,
					 circle_colors[i]);
		if (ret)
			return ret;
	}

	/* 5 – Two crossing diagonal white lines over the canvas */
	ret = st7789_draw_line(priv,   3, 82, 236, 237, COLOR_WHITE);
	if (ret)
		return ret;
	ret = st7789_draw_line(priv, 236, 82,   3, 237, COLOR_WHITE);
	if (ret)
		return ret;

	/* 6 – Small filled accent circles flanking the centre */
	ret = st7789_fill_circle(priv, 60, 130, 22, COLOR_CYAN);
	if (ret)
		return ret;
	ret = st7789_fill_circle(priv, 180, 130, 22, COLOR_MAGENTA);
	if (ret)
		return ret;

	/* 7 – Yellow bullseye at the crossing point */
	ret = st7789_fill_circle(priv, 120, 160, 8, COLOR_YELLOW);
	if (ret)
		return ret;

	return 0;
}

/* =========================================================================
 * SPI driver probe and remove
 * ========================================================================= */

static int lkss_st7789_probe(struct spi_device *spi)
{
	struct st7789_priv *priv;
	int ret;

	/* Configure SPI controller: Mode 0 (CPOL=0, CPHA=0), 40 MHz. */
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
		return ret;
	}

	/* Allocate zero-initialised private state (freed automatically on remove). */
	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->spi    = spi;
	priv->width  = ST7789_WIDTH;
	priv->height = ST7789_HEIGHT;
	spi_set_drvdata(spi, priv);

	/* Obtain GPIO descriptors from DT properties:
	 *   "reset-gpios" → priv->reset  (initial state: deasserted = HIGH)
	 *   "dc-gpios"    → priv->dc     (initial state: LOW = command mode)
	 */
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

	dev_info(&spi->dev, "ST7789: probe OK  speed=%u Hz  mode=0x%02x\n",
		 spi->max_speed_hz, spi->mode);

	/* Reset → init → draw demo */
	st7789_hw_reset(priv);

	ret = st7789_init_display(priv);
	if (ret) {
		dev_err(&spi->dev, "display init failed: %d\n", ret);
		return ret;
	}

	ret = st7789_demo(priv);
	if (ret) {
		dev_err(&spi->dev, "demo pattern failed: %d\n", ret);
		return ret;
	}

	dev_info(&spi->dev, "ST7789: 240×240 display ready\n");
	return 0;
}

static void lkss_st7789_remove(struct spi_device *spi)
{
	struct st7789_priv *priv = spi_get_drvdata(spi);

	st7789_write_cmd(priv, ST7789_DISPOFF);
	dev_info(&spi->dev, "ST7789: removed\n");
}

/* =========================================================================
 * Module / device-tree matching tables
 * ========================================================================= */

static const struct of_device_id lkss_st7789_of_match[] = {
	{ .compatible = "lkss,lkss-st7789" },
	{ }
};
MODULE_DEVICE_TABLE(of, lkss_st7789_of_match);

static const struct spi_device_id lkss_st7789_ids[] = {
	{ "lkss-st7789", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, lkss_st7789_ids);

static struct spi_driver lkss_st7789_driver = {
	.driver = {
		.name           = "lkss-st7789",
		.of_match_table = lkss_st7789_of_match,
	},
	.probe    = lkss_st7789_probe,
	.remove   = lkss_st7789_remove,
	.id_table = lkss_st7789_ids,
};
module_spi_driver(lkss_st7789_driver);

MODULE_AUTHOR("LKSS Lab Team");
MODULE_DESCRIPTION("ST7789 SPI display driver – LKSS Lab 3 full solution");
MODULE_LICENSE("GPL v2");
