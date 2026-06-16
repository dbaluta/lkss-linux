// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789_skeleton.c  –  LKSS Lab 3 SKELETON
 *
 * This file is the starting point for Lab 3.  Each TODO comment tells you
 * which exercise the step belongs to and what you need to implement.
 * Fill in the blanks exercise by exercise; the full solution is in
 * lkss_st7789.c for reference once you have finished.
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
 * Build target: CONFIG_LKSS_LAB3_ST7789_SKELETON → lkss_st7789_skeleton.ko
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
 * Given to you – do not change.
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

#define ST7789_COLMOD_RGB565  0x55   /* 16 bpp, RGB 5-6-5   */
#define ST7789_MADCTL_NORMAL  0x00   /* normal scan, RGB order */

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
 * Driver private state  –  given to you, do not change
 * ========================================================================= */
struct st7789_priv {
	struct spi_device *spi;    /* back-pointer to the SPI device     */
	struct gpio_desc  *dc;     /* D/C (data/command) GPIO descriptor */
	struct gpio_desc  *reset;  /* hardware reset GPIO descriptor     */
	u16                width;  /* panel width  in pixels             */
	u16                height; /* panel height in pixels             */
};

/* =========================================================================
 * ── EXERCISE 2 ────────────────────────────────────────────────────────────
 *
 * Implement the two fundamental SPI primitives:
 *   st7789_write_cmd()  – sets D/C LOW  then calls spi_write()
 *   st7789_write_data() – sets D/C HIGH then calls spi_write()
 *
 * These two functions are the only primitives that touch the SPI bus.
 * Everything else (init, fill, draw) calls only these two.
 * ========================================================================= */

static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	/*
	 * TODO Ex-2a: Drive D/C LOW to signal "command byte".
	 *   Use: gpiod_set_value(priv->dc, 0);
	 */

	/*
	 * TODO Ex-2b: Send the command byte over SPI (synchronous, blocks
	 *   until the byte is clocked out).
	 *   Use: return spi_write(priv->spi, &cmd, 1);
	 */
	return 0; /* remove when implemented */
}

static int st7789_write_data(struct st7789_priv *priv,
			     const u8 *buf, size_t len)
{
	/*
	 * TODO Ex-2c: Drive D/C HIGH to signal "data bytes".
	 * TODO Ex-2d: Send len bytes from buf over SPI.
	 *   Use: return spi_write(priv->spi, buf, len);
	 */
	return 0; /* remove when implemented */
}

/* Convenience wrapper – given to you */
static inline int st7789_write_data_byte(struct st7789_priv *priv, u8 byte)
{
	return st7789_write_data(priv, &byte, 1);
}

/* =========================================================================
 * ── EXERCISE 2 (continued) ────────────────────────────────────────────────
 *
 * Implement the hardware reset sequence.
 *
 * ST7789VW datasheet §8.16:
 *   • Pulse RESX LOW for ≥ 15 ms
 *   • Release RESX HIGH and wait ≥ 120 ms before the first command
 *
 * Note: gpiod_set_value() uses *logical* values.  Because the DT declares
 * this GPIO as GPIO_ACTIVE_LOW, logical 1 → physical pin LOW.
 * ========================================================================= */

static void st7789_hw_reset(struct st7789_priv *priv)
{
	/* TODO Ex-2e: Assert reset  – logical 1 → pin LOW  */

	/* TODO Ex-2f: Hold for ≥ 15 ms   (use msleep) */

	/* TODO Ex-2g: Deassert reset – logical 0 → pin HIGH */

	/* TODO Ex-2h: Wait ≥ 120 ms for the controller to boot */
}

/* =========================================================================
 * ── EXERCISE 3 ────────────────────────────────────────────────────────────
 *
 * Implement the initialization command sequence.
 *
 * After hw_reset the controller is in sleep mode.  Send the sequence below
 * to wake it up, set the pixel format to RGB565, and turn on the display.
 * Refer to the command table in the lab document (Theory, Part 4).
 * ========================================================================= */

static int st7789_init_display(struct st7789_priv *priv)
{
	int ret;

	/*
	 * TODO Ex-3a: Software reset (ST7789_SWRESET = 0x01).
	 *   After sending, wait ≥ 150 ms.
	 *   Pattern:
	 *     ret = st7789_write_cmd(priv, ST7789_SWRESET);
	 *     if (ret) return ret;
	 *     msleep(150);
	 */

	/*
	 * TODO Ex-3b: Exit sleep mode (ST7789_SLPOUT = 0x11).
	 *   Wait ≥ 500 ms after sending.
	 */

	/*
	 * TODO Ex-3c: Set pixel format to RGB565.
	 *   Command: ST7789_COLMOD (0x3A)
	 *   Data:    ST7789_COLMOD_RGB565 (0x55)
	 */

	/*
	 * TODO Ex-3d: Memory access control – normal scan, RGB colour order.
	 *   Command: ST7789_MADCTL (0x36)
	 *   Data:    ST7789_MADCTL_NORMAL (0x00)
	 */

	/*
	 * TODO Ex-3e: Display inversion on.
	 *   Command: ST7789_INVON (0x21) – no data bytes.
	 *   (Most modules need INVON for correct colours.  If colours look
	 *    wrong after this exercise, swap for ST7789_INVOFF = 0x20.)
	 */

	/*
	 * TODO Ex-3f: Normal display mode on.
	 *   Command: ST7789_NORON (0x13) – no data bytes.
	 */

	/*
	 * TODO Ex-3g: Display output on.
	 *   Command: ST7789_DISPON (0x29) – no data bytes.
	 *   Wait ≥ 100 ms after sending.
	 */

	(void)ret; /* suppress unused-variable warning until TODOs are filled */
	return 0;
}

/* =========================================================================
 * ── EXERCISE 3 (continued) ────────────────────────────────────────────────
 *
 * Implement the address-window function and the full-screen fill.
 *
 * st7789_set_addr_win():
 *   Sends CASET (column/X range) and RASET (row/Y range) commands, then
 *   issues RAMWR.  All following write_data() calls fill the window.
 *   CASET and RASET each take 4 bytes: [x0>>8, x0&0xff, x1>>8, x1&0xff].
 *
 * st7789_fill():
 *   Sets the full-panel window, then sends 'height' scanlines of pixel data.
 *   Each pixel is 2 bytes: [colour >> 8, colour & 0xff] (big-endian RGB565).
 * ========================================================================= */

static int st7789_set_addr_win(struct st7789_priv *priv,
			       u16 x0, u16 y0, u16 x1, u16 y1)
{
	u8 col[4] = { x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff };
	u8 row[4] = { y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff };
	int ret;

	/* TODO Ex-3h: Send CASET command followed by col[4] as data. */

	/* TODO Ex-3i: Send RASET command followed by row[4] as data. */

	/* TODO Ex-3j: Send RAMWR command (no data bytes here;
	 *             pixel data follows in subsequent write_data calls).
	 *   return st7789_write_cmd(priv, ST7789_RAMWR);
	 */

	(void)col; (void)row; (void)ret;
	return 0;
}

static int st7789_fill(struct st7789_priv *priv, u16 color)
{
	u8 color_hi = color >> 8;
	u8 color_lo = color & 0xff;
	u8 *line;
	int ret = 0, x, y;

	/* TODO Ex-3k: Set address window to the full panel:
	 *   st7789_set_addr_win(priv, 0, 0, priv->width-1, priv->height-1);
	 */

	/* Allocate one scanline buffer – given to you */
	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	/* Pre-fill the buffer with the repeated colour (big-endian per pixel).
	 * Given to you.
	 */
	for (x = 0; x < priv->width; x++) {
		line[x * 2]     = color_hi;
		line[x * 2 + 1] = color_lo;
	}

	/*
	 * TODO Ex-3l: Send 'height' scanlines using st7789_write_data().
	 *   for (y = 0; y < priv->height; y++) {
	 *       ret = st7789_write_data(priv, line, priv->width * 2);
	 *       if (ret) break;
	 *   }
	 */
	(void)y;

	kfree(line);
	return ret;
}

/* =========================================================================
 * ── EXERCISE 4 ────────────────────────────────────────────────────────────
 *
 * Implement st7789_fill_rect() and then use it to draw a rainbow striped
 * test card (8 coloured stripes of 30 px each across the full width).
 *
 * fill_rect() is identical to fill() except the address window is set to
 * [x, y]–[x+w-1, y+h-1] instead of the full panel.  Remember to clamp
 * coordinates that go outside the panel boundaries.
 * ========================================================================= */

static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	u8 color_hi = color >> 8;
	u8 color_lo = color & 0xff;
	u8 *line;
	u16 i, row;
	int ret = 0;

	/* Clamp to panel boundaries – given to you */
	if (x >= priv->width || y >= priv->height)
		return 0;
	if (x + w > priv->width)
		w = priv->width - x;
	if (y + h > priv->height)
		h = priv->height - y;
	if (!w || !h)
		return 0;

	/* TODO Ex-4a: Set address window to [x,y]–[x+w-1, y+h-1]. */

	/* Allocate one-row buffer – given to you */
	line = kmalloc(w * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	/* TODO Ex-4b: Fill line buffer with the colour (big-endian 2 bytes/px).
	 *   for (i = 0; i < w; i++) {
	 *       line[i*2]   = color_hi;
	 *       line[i*2+1] = color_lo;
	 *   }
	 */
	(void)i; (void)color_hi; (void)color_lo;

	/* TODO Ex-4c: Send h scanlines using st7789_write_data().
	 *   for (row = 0; row < h; row++) {
	 *       ret = st7789_write_data(priv, line, w * 2);
	 *       if (ret) break;
	 *   }
	 */
	(void)row;

	kfree(line);
	return ret;
}

/*
 * st7789_rainbow_stripes - draw 8 coloured horizontal stripes
 *
 * TODO Ex-4d: Fill this function body.
 * Draw 8 stripes of 30 px height each, covering the full panel width.
 * Use the colour array below and call st7789_fill_rect() for each stripe.
 * Stripe i covers rows [i*30 .. i*30+29].
 */
static int st7789_rainbow_stripes(struct st7789_priv *priv)
{
	static const u16 colors[8] = {
		COLOR_RED,  COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN,
		COLOR_CYAN, COLOR_BLUE,   COLOR_MAGENTA, COLOR_WHITE,
	};
	int i, ret;

	/* TODO Ex-4d: loop over colors[], draw each stripe */
	(void)i; (void)ret; (void)colors;
	return 0;
}

/* =========================================================================
 * ── EXERCISE 5 ────────────────────────────────────────────────────────────
 *
 * Implement pixel-level drawing primitives.
 *
 * st7789_draw_pixel(): 1×1 address window + 2-byte colour write.
 *
 * st7789_draw_line(): Bresenham integer line algorithm.
 *   dx = |x1-x0|,  dy = -|y1-y0|,  err = dx+dy
 *   Each step: e2 = 2*err
 *     if e2 >= dy  → err += dy;  x0 += sx
 *     if e2 <= dx  → err += dx;  y0 += sy
 *   Stop when (x0,y0) == (x1,y1).
 *
 * st7789_draw_circle(): Midpoint circle algorithm.
 *   Start: x=0, y=r, d=1-r
 *   Each step: draw 8 symmetric points, then
 *     if d < 0 → d += 2*x+3
 *     else     → d += 2*(x-y)+5;  y--
 *   Stop when x > y.
 * ========================================================================= */

static int st7789_draw_pixel(struct st7789_priv *priv,
			     u16 x, u16 y, u16 color)
{
	u8 pixel[2] = { color >> 8, color & 0xff };
	int ret;

	if (x >= priv->width || y >= priv->height)
		return 0;

	/*
	 * TODO Ex-5a: Set address window to single pixel [x,y]–[x,y].
	 * TODO Ex-5b: Send the 2-byte pixel[] array as data.
	 */
	(void)pixel; (void)ret;
	return 0;
}

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
		/* TODO Ex-5c: Draw pixel at (x0, y0). */

		if (x0 == x1 && y0 == y1)
			break;

		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
	(void)ret;
	return 0;
}

static int st7789_draw_circle(struct st7789_priv *priv,
			      int cx, int cy, int r, u16 color)
{
	int x = 0, y = r, d = 1 - r, ret;

#define PLOT(px, py) do {						\
	ret = st7789_draw_pixel(priv, (u16)(px), (u16)(py), color);	\
	if (ret) return ret;						\
} while (0)

	while (x <= y) {
		/*
		 * TODO Ex-5d: Draw the 8 symmetric points of the circle.
		 *   The eight octant-symmetric points for (x, y) are:
		 *   (cx+x, cy+y), (cx-x, cy+y), (cx+x, cy-y), (cx-x, cy-y)
		 *   (cx+y, cy+x), (cx-y, cy+x), (cx+y, cy-x), (cx-y, cy-x)
		 *   Use the PLOT macro above.
		 */

		/* Decision parameter update – given to you */
		if (d < 0)
			d += 2 * x + 3;
		else { d += 2 * (x - y) + 5; y--; }
		x++;
	}
#undef PLOT
	(void)ret;
	return 0;
}

/*
 * st7789_fill_circle - given to you; study it to understand how fill_rect
 *                      is used as a building block for filled shapes.
 *
 * For each row dy in [-r, r]: compute the chord half-width dx using the
 * Pythagorean theorem, then fill a horizontal strip of 2*dx+1 pixels.
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

/*
 * st7789_demo - draw the final test pattern
 *
 * TODO Ex-5e: Implement this function.  It should:
 *   1. Fill the display black.
 *   2. Draw rainbow stripes across the top (call st7789_rainbow_stripes,
 *      but only rows 0..79 – modify the stripe heights or use fill_rect
 *      directly with 8 stripes of 10 px each).
 *   3. Add a white 2-pixel border around the full panel.
 *   4. Draw 5 concentric circle outlines centred at (120, 160),
 *      radii 70, 55, 40, 25, 10 in alternating colours.
 *   5. Draw two white diagonal lines crossing at (120, 160):
 *      (3,82)→(236,237) and (236,82)→(3,237).
 *   6. Draw two filled circles: cyan at (60,130) r=22,
 *      magenta at (180,130) r=22.
 *   7. Draw a small yellow filled circle at (120,160) r=8 (bullseye).
 */
static int st7789_demo(struct st7789_priv *priv)
{
	/* TODO Ex-5e */
	return 0;
}

/* =========================================================================
 * SPI driver probe and remove
 * ========================================================================= */

static int lkss_st7789_probe(struct spi_device *spi)
{
	struct st7789_priv *priv;
	int ret;

	/*
	 * TODO Ex-1a: Configure SPI controller mode and speed.
	 *   spi->mode = SPI_MODE_0;
	 *   ret = spi_setup(spi);
	 *   if (ret < 0) {
	 *       dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
	 *       return ret;
	 *   }
	 */

	/*
	 * TODO Ex-1b: Allocate zero-initialised private state.
	 *   priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	 *   if (!priv) return -ENOMEM;
	 *   priv->spi    = spi;
	 *   priv->width  = ST7789_WIDTH;
	 *   priv->height = ST7789_HEIGHT;
	 *   spi_set_drvdata(spi, priv);
	 */
	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->spi    = spi;
	priv->width  = ST7789_WIDTH;
	priv->height = ST7789_HEIGHT;
	spi_set_drvdata(spi, priv);

	/*
	 * TODO Ex-2i: Obtain the GPIO descriptor for the reset line.
	 *   priv->reset = devm_gpiod_get(&spi->dev, "reset", GPIOD_OUT_HIGH);
	 *   if (IS_ERR(priv->reset)) {
	 *       dev_err(&spi->dev, "failed to get reset GPIO: %ld\n",
	 *               PTR_ERR(priv->reset));
	 *       return PTR_ERR(priv->reset);
	 *   }
	 */

	/*
	 * TODO Ex-2j: Obtain the GPIO descriptor for the D/C line.
	 *   priv->dc = devm_gpiod_get(&spi->dev, "dc", GPIOD_OUT_LOW);
	 *   if (IS_ERR(priv->dc)) { ... }
	 */

	dev_info(&spi->dev, "lkss-st7789-skeleton: probe OK  speed=%u Hz\n",
		 spi->max_speed_hz);

	/*
	 * TODO Ex-2k: After implementing hw_reset and write_cmd, uncomment:
	 *   st7789_hw_reset(priv);
	 */

	/*
	 * TODO Ex-3m: After implementing init_display, uncomment:
	 *   ret = st7789_init_display(priv);
	 *   if (ret) return ret;
	 *   ret = st7789_fill(priv, COLOR_BLUE);
	 *   if (ret) return ret;
	 */

	/*
	 * TODO Ex-4e: After implementing fill_rect and rainbow_stripes:
	 *   Replace the fill above with: st7789_rainbow_stripes(priv);
	 */

	/*
	 * TODO Ex-5f: After implementing all drawing primitives:
	 *   ret = st7789_demo(priv);
	 *   if (ret) return ret;
	 */

	(void)ret;
	return 0;
}

static void lkss_st7789_remove(struct spi_device *spi)
{
	/* priv is available if you need to send DISPOFF here */
	dev_info(&spi->dev, "lkss-st7789-skeleton: removed\n");
}

/* =========================================================================
 * Module / device-tree matching tables  –  given to you, do not change
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

MODULE_AUTHOR("LKSS Student");
MODULE_DESCRIPTION("ST7789 SPI display driver – LKSS Lab 3 skeleton");
MODULE_LICENSE("GPL v2");
