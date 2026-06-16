// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_st7789_sol.c - LKSS Lab 3: ST7789 SPI display driver, REFERENCE SOLUTION
 *
 * Complete, working implementation of every TODO in lkss_st7789.c.  Use this
 * module to verify the hardware (wiring, pinctrl, device tree node) before
 * or after working through the skeleton, or to compare against your own
 * implementation.
 *
 * Hardware connections on the i.MX93 FRDM EXT2 header (J601):
 *
 *   Display pin  J601 pin  SoC pad      Notes
 *   -----------  --------  -----------  ------------------------------------
 *   SDA (MOSI)   19        GPIO_IO10    LPSPI3_SOUT
 *   SCL (SCK)    23        GPIO_IO11    LPSPI3_SCK
 *   CS           GND       --           Module CS tied low (always selected)
 *   RES (RST)    32        GPIO_IO12    Active-low hardware reset
 *   DC  (D/C)    7         GPIO_IO04    Low = command, High = data
 *   VCC          1         3.3 V        Logic and panel supply
 *   GND          6         GND
 *   BLK          1         3.3 V        Backlight (tie high for always-on)
 *
 * Device Tree compatible string: "lkss,st7789"
 *
 * Cross-compile:
 *   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
 *        M=drivers/lkss/labs/lab3 -j$(nproc)
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/kernel.h>   /* abs(), int_sqrt(), clamp(), swap() */
#include <linux/slab.h>     /* kmalloc(), kfree() */
#include <linux/of.h>
#include <linux/fixp-arith.h> /* fixp_sin16() -- no FPU in kernel space */
#include <linux/random.h>    /* get_random_u32() */

/* ------------------------------------------------------------------
 * ST7789 command opcodes (ST7789VW datasheet, chapter 9)
 * ------------------------------------------------------------------ */
#define ST7789_SWRESET   0x01  /* software reset                      */
#define ST7789_SLPOUT    0x11  /* sleep out                           */
#define ST7789_NORON     0x13  /* normal display mode on              */
#define ST7789_INVON     0x21  /* display inversion on                */
#define ST7789_DISPOFF   0x28  /* display off                         */
#define ST7789_DISPON    0x29  /* display on                          */
#define ST7789_CASET     0x2A  /* column address set                  */
#define ST7789_RASET     0x2B  /* row address set                     */
#define ST7789_RAMWR     0x2C  /* memory write                        */
#define ST7789_MADCTL    0x36  /* memory data access control          */
#define ST7789_COLMOD    0x3A  /* interface pixel format              */
#define ST7789_PORCTRL   0xB2  /* porch setting                       */
#define ST7789_GCTRL     0xB7  /* gate control (VGH/VGL)              */
#define ST7789_VCOMS     0xBB  /* VCOM setting                        */
#define ST7789_VDVVRHEN  0xC2  /* VDV and VRH command enable          */
#define ST7789_VRHS      0xC3  /* VRH (gamma reference voltage) set   */
#define ST7789_VDVS      0xC4  /* VDV set                             */
#define ST7789_VCMOFSET  0xC5  /* VCOM offset set                     */
#define ST7789_PWCTRL1   0xD0  /* power control 1 (AVDD/AVCL/VDS)     */
#define ST7789_PVGAMCTRL 0xE0  /* positive voltage gamma control      */
#define ST7789_NVGAMCTRL 0xE1  /* negative voltage gamma control      */

/*
 * Only the low nibble (bits 2:0, MCU/control interface format) matters
 * in 4-wire SPI mode -- the high nibble selects the (unused) parallel
 * RGB interface format.  fb_st7789v.c sends MIPI_DCS_PIXEL_FMT_16BIT
 * (5) unshifted, i.e. 0x05; matched here exactly.
 */
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
};

/*
 * ST7789_TRACE - compile-time switch for the verbose cmd/data dump below.
 *
 * Every write_cmd()/write_data() call goes through the UART console
 * *synchronously* -- dev_info() blocks until the line has actually been
 * transmitted.  At typical console baud rates (115200) that is a real
 * handful of milliseconds per line, multiplied by hundreds of calls during
 * init + a 240-row fill.  That incidental delay was acting as unintended
 * extra margin on top of the explicit msleep()s.  Set to 1 only when you
 * need the trace for debugging; leave at 0 for normal/timing-sensitive runs.
 */
#define ST7789_TRACE 0

/*
 * st7789_trace - log a command/data byte sequence, for side-by-side
 * comparison against fbtft's DEBUG_WRITE_REGISTER trace
 * (insmod fb_st7789v.ko debug=0x200000, or 0xffffffff for everything).
 * Long buffers (full scanlines of pixel data) are summarized by length
 * only, to avoid flooding the log.
 */
#if ST7789_TRACE
static void st7789_trace(struct st7789_priv *priv, const char *what,
			 const u8 *buf, size_t len)
{
	char hex[3 * 16 + 1] = "";
	size_t i, n = min(len, (size_t)16);

	for (i = 0; i < n; i++)
		scnprintf(hex + i * 3, 4, "%02x ", buf[i]);

	if (len > 16)
		dev_info(&priv->spi->dev, "%-4s: <%zu bytes>\n", what, len);
	else
		dev_info(&priv->spi->dev, "%-4s: %s\n", what, hex);
}
#else
static inline void st7789_trace(struct st7789_priv *priv, const char *what,
				const u8 *buf, size_t len)
{
}
#endif

static int st7789_write_cmd(struct st7789_priv *priv, u8 cmd)
{
	st7789_trace(priv, "cmd", &cmd, 1);
	gpiod_set_value(priv->dc, 0);
	return spi_write(priv->spi, &cmd, 1);
}

static int st7789_write_data(struct st7789_priv *priv,
			     const u8 *buf, size_t len)
{
	st7789_trace(priv, "data", buf, len);
	gpiod_set_value(priv->dc, 1);
	return spi_write(priv->spi, buf, len);
}

static inline int st7789_write_data_byte(struct st7789_priv *priv, u8 byte)
{
	return st7789_write_data(priv, &byte, 1);
}

static void st7789_hw_reset(struct st7789_priv *priv)
{
#if ST7789_TRACE
	dev_info(&priv->spi->dev, "reset: assert (initial logical value at probe was HIGH)\n");
#endif
	gpiod_set_value(priv->reset, 1);
	msleep(20);   /* datasheet minimum is 15ms; +margin now that the
		       * console trace no longer eats a few ms for free */
#if ST7789_TRACE
	dev_info(&priv->spi->dev, "reset: deassert, then wait 150ms\n");
#endif
	gpiod_set_value(priv->reset, 0);
	msleep(150);  /* datasheet minimum is 120ms; +margin, see above */
}

static int st7789_init_display(struct st7789_priv *priv)
{
	/*
	 * Porch/gate/power/VCOM/gamma-reference values below match the
	 * HSD20_IPS panel profile in drivers/staging/fbtft/fb_st7789v.c,
	 * which is confirmed working on this board.  Without them the
	 * controller still accepts every command (no SPI errors) but the
	 * default VGH/VGL/VCOM/VRH levels after reset don't swing the
	 * liquid crystal enough to produce a visible image.
	 */
	static const u8 porctrl[]  = { 0x05, 0x05, 0x00, 0x33, 0x33 };
	static const u8 vdvvrhen[] = { 0x01, 0xFF };
	static const u8 pwctrl1[]  = { 0xA4, 0xA1 };
	static const u8 pvgamctrl[] = { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05, 0x2E,
					 0x44, 0x45, 0x0F, 0x17, 0x16, 0x2B, 0x33 };
	static const u8 nvgamctrl[] = { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05, 0x2E,
					 0x43, 0x45, 0x0F, 0x16, 0x16, 0x2B, 0x33 };
	int ret;

	/*
	 * No SWRESET here: drivers/staging/fbtft/fb_st7789v.c -- proven
	 * working on this exact panel/wiring -- relies solely on the
	 * hardware RESX pulse (st7789_hw_reset() above) and never sends a
	 * software SWRESET.  That is the one remaining behavioral
	 * difference found after a byte-for-byte comparison of the two
	 * init sequences, so it's dropped here too rather than risk an
	 * interaction between the two reset paths on this particular
	 * (possibly clone) panel.
	 */
	ret = st7789_write_cmd(priv, ST7789_SLPOUT);
	if (ret) return ret;
	msleep(600);  /* datasheet minimum 500ms; +margin (see ST7789_TRACE) */

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
	msleep(150);  /* datasheet minimum 100ms; +margin (see ST7789_TRACE) */

	ret = st7789_write_cmd(priv, ST7789_INVON);
	if (ret) return ret;

	ret = st7789_write_cmd(priv, ST7789_MADCTL);
	if (ret) return ret;
	ret = st7789_write_data_byte(priv, ST7789_MADCTL_NORMAL);
	if (ret) return ret;

	/*
	 * Gamma reference curves -- without these the panel is left at its
	 * post-reset default transfer function, which on this IPS variant
	 * produces no visible contrast even though every SPI transfer
	 * succeeds.  Values are fb_st7789v.c's HSD20_IPS_GAMMA, taken
	 * directly from its dmesg trace (debug=DEBUG_WRITE_REGISTER).
	 */
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
	u8 color_hi = color >> 8;
	u8 color_lo = color & 0xff;
	u8 *line;
	int ret = 0, x, y;

	ret = st7789_set_addr_win(priv, 0, 0, priv->width - 1, priv->height - 1);
	if (ret) return ret;

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line) return -ENOMEM;

	for (x = 0; x < priv->width; x++) {
		line[x * 2]     = color_hi;
		line[x * 2 + 1] = color_lo;
	}

	for (y = 0; y < priv->height; y++) {
		ret = st7789_write_data(priv, line, priv->width * 2);
		if (ret) break;
	}

	kfree(line);
	return ret;
}

static int st7789_fill_rect(struct st7789_priv *priv,
			    u16 x, u16 y, u16 w, u16 h, u16 color)
{
	u8 color_hi = color >> 8;
	u8 color_lo = color & 0xff;
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
		line[i * 2]     = color_hi;
		line[i * 2 + 1] = color_lo;
	}

	for (row = 0; row < h; row++) {
		ret = st7789_write_data(priv, line, w * 2);
		if (ret) break;
	}

	kfree(line);
	return ret;
}

static int st7789_draw_pixel(struct st7789_priv *priv,
			     u16 x, u16 y, u16 color)
{
	u8 pixel[2] = { color >> 8, color & 0xff };
	int ret;

	if (x >= priv->width || y >= priv->height) return 0;

	ret = st7789_set_addr_win(priv, x, y, x, y);
	if (ret) return ret;

	return st7789_write_data(priv, pixel, 2);
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
		ret = st7789_draw_pixel(priv, (u16)x0, (u16)y0, color);
		if (ret) return ret;

		if (x0 == x1 && y0 == y1) break;

		e2 = 2 * err;
		if (e2 >= dy) { if (x0 == x1) break; err += dy; x0 += sx; }
		if (e2 <= dx) { if (y0 == y1) break; err += dx; y0 += sy; }
	}
	return 0;
}

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

		if (d < 0) {
			d += 2 * x + 3;
		} else {
			d += 2 * (x - y) + 5;
			y--;
		}
		x++;
	}
#undef PLOT
	return 0;
}

static int st7789_fill_circle(struct st7789_priv *priv,
			      int cx, int cy, int r, u16 color)
{
	int dy, dx, ret;

	for (dy = -r; dy <= r; dy++) {
		dx = (int)int_sqrt((u32)(r * r - dy * dy));
		ret = st7789_fill_rect(priv,
				       (u16)(cx - dx), (u16)(cy + dy),
				       (u16)(2 * dx + 1), 1, color);
		if (ret) return ret;
	}
	return 0;
}

/*
 * Demo pattern, in increasing order of difficulty -- and, more
 * importantly, in increasing order of "the picture is generated by an
 * idea" rather than "the picture is a list of hardcoded shapes":
 *
 *   0. Quick white/red/yellow/green flash    -- is the panel alive at all?
 *   1. st7789_demo_full()                    -- every static primitive once
 *   2. st7789_demo_gradient()                -- color computed from x
 *   3. st7789_demo_checkerboard()             -- layout computed from (x,y)
 *   4. st7789_demo_sine()                    -- the picture *is* a function
 *   5. st7789_demo_plasma()                  -- per-pixel + animation + buffering
 *   6. st7789_demo_bounce()                  -- state carried across frames
 *   7. st7789_demo_life()                    -- simulation + partial redraw
 *
 * Each tier is its own function so it can be studied, modified, or
 * skipped independently; st7789_demo() just calls them one by one.
 */
#define DEMO_STEP_DELAY_MS       1000 /* one-shot steps: long enough to look at  */
#define DEMO_ANIM_FRAME_DELAY_MS   40 /* plasma/bounce: smooth without flooding SPI */
#define DEMO_LIFE_GEN_DELAY_MS    200 /* Game of Life: slow enough to watch evolve */

static int st7789_demo_full(struct st7789_priv *priv);
static int st7789_demo_gradient(struct st7789_priv *priv);
static int st7789_demo_checkerboard(struct st7789_priv *priv);
static int st7789_demo_sine(struct st7789_priv *priv);
static int st7789_demo_plasma(struct st7789_priv *priv);
static int st7789_demo_bounce(struct st7789_priv *priv);
static int st7789_demo_life(struct st7789_priv *priv);

static int st7789_demo(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	static const struct { u16 color; const char *name; } steps[] = {
		{ 0xFFFF, "white"  },
		{ 0xF800, "red"    },
		{ 0xFFE0, "yellow" },
		{ 0x07E0, "green"  },
	};
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(steps); i++) {
		dev_info(dev, "demo: solid %s\n", steps[i].name);
		ret = st7789_fill(priv, steps[i].color);
		if (ret) return ret;
		msleep(DEMO_STEP_DELAY_MS);
	}

	ret = st7789_demo_full(priv);
	if (ret) return ret;

	ret = st7789_demo_gradient(priv);
	if (ret) return ret;

	ret = st7789_demo_checkerboard(priv);
	if (ret) return ret;

	ret = st7789_demo_sine(priv);
	if (ret) return ret;

	ret = st7789_demo_plasma(priv);
	if (ret) return ret;

	ret = st7789_demo_bounce(priv);
	if (ret) return ret;

	ret = st7789_demo_life(priv);
	if (ret) return ret;

	dev_info(dev, "demo: done\n");
	return 0;
}

/*
 * st7789_demo_full - the original colorful demo exercising every primitive:
 *   black background, red border, green filled circle, blue rectangle,
 *   white diagonal line, yellow circle outline.  Chained from st7789_demo()
 *   above, after the quick color-flash sanity check.
 */
static int st7789_demo_full(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	int ret;

	dev_info(dev, "demo: black background\n");
	ret = st7789_fill(priv, 0x0000);
	if (ret) return ret;
	msleep(DEMO_STEP_DELAY_MS);

	dev_info(dev, "demo: red border\n");
	st7789_fill_rect(priv,   0,   0, 240,   4, 0xF800);
	st7789_fill_rect(priv,   0, 236, 240,   4, 0xF800);
	st7789_fill_rect(priv,   0,   0,   4, 240, 0xF800);
	st7789_fill_rect(priv, 236,   0,   4, 240, 0xF800);
	msleep(DEMO_STEP_DELAY_MS);

	dev_info(dev, "demo: green filled circle\n");
	ret = st7789_fill_circle(priv, 60, 60, 50, 0x07E0);
	if (ret) return ret;
	msleep(DEMO_STEP_DELAY_MS);

	dev_info(dev, "demo: blue rectangle\n");
	ret = st7789_fill_rect(priv, 130, 130, 100, 100, 0x001F);
	if (ret) return ret;
	msleep(DEMO_STEP_DELAY_MS);

	dev_info(dev, "demo: white diagonal line\n");
	ret = st7789_draw_line(priv, 5, 5, 234, 234, 0xFFFF);
	if (ret) return ret;
	msleep(DEMO_STEP_DELAY_MS);

	dev_info(dev, "demo: yellow circle outline\n");
	ret = st7789_draw_circle(priv, 120, 120, 40, 0xFFE0);
	if (ret) return ret;
	msleep(DEMO_STEP_DELAY_MS);

	return 0;
}

/* ==================================================================
 * Tier 1 -- procedural patterns: same primitives as st7789_demo_full()
 * above, but the colors/layout are COMPUTED from (x, y) instead of
 * typed in as constants.  No new APIs to learn -- the point is that
 * this is the first demo where you have to actually understand the
 * RGB565 bit layout (5 bits red, 6 bits green, 5 bits blue) instead of
 * copy-pasting a hex value from a table.
 * ================================================================== */

/*
 * st7789_demo_gradient - three stacked color ramps (red/green/blue),
 * each column's intensity computed from its x position.
 */
static int st7789_demo_gradient(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	const u16 band_h = priv->height / 3;
	int x, ret;

	dev_info(dev, "demo: RGB gradient bands\n");

	for (x = 0; x < priv->width; x++) {
		u8 level5 = x * 31 / (priv->width - 1); /* 0..31, 5-bit channel */
		u8 level6 = x * 63 / (priv->width - 1); /* 0..63, 6-bit channel */
		u16 red   = level5 << 11;
		u16 green = level6 << 5;
		u16 blue  = level5;

		ret = st7789_fill_rect(priv, x, 0, 1, band_h, red);
		if (ret) return ret;
		ret = st7789_fill_rect(priv, x, band_h, 1, band_h, green);
		if (ret) return ret;
		ret = st7789_fill_rect(priv, x, 2 * band_h, 1,
				       priv->height - 2 * band_h, blue);
		if (ret) return ret;
	}
	msleep(DEMO_STEP_DELAY_MS);
	return 0;
}

/*
 * st7789_demo_checkerboard - the layout itself is generated from a
 * parity formula, (cell_x + cell_y) % 2, instead of being a fixed
 * list of rectangles.
 */
static int st7789_demo_checkerboard(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	const u16 cell = 16;
	u16 cx, cy;
	int ret;

	dev_info(dev, "demo: checkerboard\n");

	for (cy = 0; cy * cell < priv->height; cy++) {
		for (cx = 0; cx * cell < priv->width; cx++) {
			u16 color = ((cx + cy) % 2) ? 0xFFFF : 0x0000;

			ret = st7789_fill_rect(priv, cx * cell, cy * cell,
					       cell, cell, color);
			if (ret) return ret;
		}
	}
	msleep(DEMO_STEP_DELAY_MS);
	return 0;
}

/* ==================================================================
 * Tier 2 -- math made visible.  fixp_sin16() (linux/fixp-arith.h) is
 * the kernel's fixed-point sine helper: there is no FPU available in
 * kernel space, so this is the standard way to get a sine wave
 * without pulling in soft-float emulation.  It takes a whole-number
 * angle in degrees and returns a value in [-0x7fff, 0x7fff].
 * ================================================================== */

/*
 * st7789_demo_sine - plot y = sin(x) across the full panel width.
 * First demo where the picture genuinely *is* the math, one column
 * at a time, rather than a shape drawn at fixed coordinates.
 */
static int st7789_demo_sine(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	const int mid = priv->height / 2;
	const int amp = priv->height / 2 - 10;
	int x, ret;

	dev_info(dev, "demo: sine wave plot\n");

	ret = st7789_fill(priv, 0x0000);
	if (ret) return ret;

	for (x = 0; x < priv->width; x++) {
		int degrees = x * 360 / priv->width;
		int y = mid - (fixp_sin16(degrees) * amp) / 0x7fff;

		ret = st7789_draw_pixel(priv, x, y, 0x07FF);     /* cyan */
		if (ret) return ret;
		ret = st7789_draw_pixel(priv, x, y + 1, 0x07FF); /* 2px thick */
		if (ret) return ret;
	}
	msleep(DEMO_STEP_DELAY_MS);
	return 0;
}

/*
 * st7789_demo_plasma - classic demo-scene "plasma" effect: each
 * pixel's color comes from summing three phase-shifted sine waves of
 * (x, y, time).  Animated by advancing the phase every frame.
 *
 * Unlike the per-column st7789_fill_rect() calls in the tiers above,
 * this builds one full scanline buffer per row (like st7789_fill()
 * does) instead of issuing one SPI transaction per pixel -- 240
 * single-pixel writes per row would be ~240x more SPI transactions
 * for no benefit.  This is the first real "why does buffering matter"
 * lesson in the lab.
 */
#define PLASMA_FRAMES 40

static int st7789_demo_plasma(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	u8 *line;
	int frame, x, y, ret = 0;

	dev_info(dev, "demo: plasma (%d frames)\n", PLASMA_FRAMES);

	line = kmalloc(priv->width * 2, GFP_KERNEL);
	if (!line)
		return -ENOMEM;

	for (frame = 0; frame < PLASMA_FRAMES; frame++) {
		int phase = frame * 9; /* degrees advanced per frame */

		ret = st7789_set_addr_win(priv, 0, 0,
					  priv->width - 1, priv->height - 1);
		if (ret) break;

		for (y = 0; y < priv->height; y++) {
			for (x = 0; x < priv->width; x++) {
				int v = fixp_sin16(x * 2 + phase) +
					fixp_sin16(y * 3 - phase) +
					fixp_sin16((x + y) * 2 + phase * 2);
				int level = v / 3 + 0x7fff; /* ~0..0xfffe */
				u8 level5, level6;
				u16 color;

				if (level < 0) level = 0;
				if (level > 0xfffe) level = 0xfffe;
				level5 = level * 31 / 0xfffe;
				level6 = level * 63 / 0xfffe;
				color = (level5 << 11) | (level6 << 5) | level5;

				line[x * 2]     = color >> 8;
				line[x * 2 + 1] = color & 0xff;
			}
			ret = st7789_write_data(priv, line, priv->width * 2);
			if (ret) break;
		}
		if (ret) break;
		msleep(DEMO_ANIM_FRAME_DELAY_MS);
	}

	kfree(line);
	return ret;
}

/* ==================================================================
 * Tier 3 -- state carried across frames.  The picture is no longer a
 * pure function of (x, y); each frame depends on the previous one.
 * Erase-old/draw-new ordering matters here -- get it backwards and
 * you get visible flicker or trails, a real bug students will hit and
 * have to reason about, not just "did the shape appear".
 * ================================================================== */

#define BOUNCE_FRAMES 90
#define BOUNCE_RADIUS 12

/*
 * st7789_demo_bounce - a ball bounces around the panel.  Position and
 * velocity are carried across loop iterations; each frame erases the
 * ball at its old position before drawing it at the new one.
 */
static int st7789_demo_bounce(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	int x = priv->width / 2, y = priv->height / 2;
	int vx = 5, vy = 3;
	int frame, ret;

	dev_info(dev, "demo: bouncing ball (%d frames)\n", BOUNCE_FRAMES);

	ret = st7789_fill(priv, 0x0000);
	if (ret) return ret;

	for (frame = 0; frame < BOUNCE_FRAMES; frame++) {
		ret = st7789_fill_circle(priv, x, y, BOUNCE_RADIUS, 0x0000);
		if (ret) return ret;

		x += vx;
		y += vy;
		if (x - BOUNCE_RADIUS < 0 || x + BOUNCE_RADIUS >= priv->width)
			vx = -vx;
		if (y - BOUNCE_RADIUS < 0 || y + BOUNCE_RADIUS >= priv->height)
			vy = -vy;
		x = clamp(x, BOUNCE_RADIUS, priv->width  - 1 - BOUNCE_RADIUS);
		y = clamp(y, BOUNCE_RADIUS, priv->height - 1 - BOUNCE_RADIUS);

		ret = st7789_fill_circle(priv, x, y, BOUNCE_RADIUS, 0xFFE0);
		if (ret) return ret;

		msleep(DEMO_ANIM_FRAME_DELAY_MS);
	}
	return 0;
}

#define LIFE_CELL        8
#define LIFE_COLS        (ST7789_WIDTH  / LIFE_CELL)
#define LIFE_ROWS        (ST7789_HEIGHT / LIFE_CELL)
#define LIFE_GENERATIONS 60

/*
 * st7789_demo_life - Conway's Game of Life.  Simulation state (the
 * alive/dead grid) is kept completely separate from the display, and
 * -- the real lesson here -- each generation redraws only the cells
 * that actually changed instead of repainting the whole panel.  The
 * board wraps around at the edges (toroidal) so colonies keep
 * evolving instead of dying out at the borders.
 */
static int st7789_demo_life(struct st7789_priv *priv)
{
	struct device *dev = &priv->spi->dev;
	u8 *cur, *next;
	int gen, cx, cy, ret = 0;

	dev_info(dev, "demo: Conway's Game of Life (%d generations)\n",
		LIFE_GENERATIONS);

	cur  = kmalloc(LIFE_COLS * LIFE_ROWS, GFP_KERNEL);
	next = kmalloc(LIFE_COLS * LIFE_ROWS, GFP_KERNEL);
	if (!cur || !next) {
		kfree(cur);
		kfree(next);
		return -ENOMEM;
	}

	ret = st7789_fill(priv, 0x0000);
	if (ret) goto out;

	/* random ~30% initial population */
	for (cy = 0; cy < LIFE_ROWS; cy++) {
		for (cx = 0; cx < LIFE_COLS; cx++) {
			u8 alive = (get_random_u32() % 10) < 3;

			cur[cy * LIFE_COLS + cx] = alive;
			if (alive) {
				ret = st7789_fill_rect(priv, cx * LIFE_CELL,
						       cy * LIFE_CELL,
						       LIFE_CELL, LIFE_CELL,
						       0x07E0);
				if (ret) goto out;
			}
		}
	}
	msleep(DEMO_LIFE_GEN_DELAY_MS);

	for (gen = 0; gen < LIFE_GENERATIONS; gen++) {
		for (cy = 0; cy < LIFE_ROWS; cy++) {
			for (cx = 0; cx < LIFE_COLS; cx++) {
				int n = 0, dx, dy;

				for (dy = -1; dy <= 1; dy++) {
					for (dx = -1; dx <= 1; dx++) {
						int nx, ny;

						if (!dx && !dy)
							continue;
						nx = (cx + dx + LIFE_COLS) % LIFE_COLS;
						ny = (cy + dy + LIFE_ROWS) % LIFE_ROWS;
						n += cur[ny * LIFE_COLS + nx];
					}
				}

				if (cur[cy * LIFE_COLS + cx])
					next[cy * LIFE_COLS + cx] = (n == 2 || n == 3);
				else
					next[cy * LIFE_COLS + cx] = (n == 3);
			}
		}

		/* redraw only the cells that actually changed */
		for (cy = 0; cy < LIFE_ROWS; cy++) {
			for (cx = 0; cx < LIFE_COLS; cx++) {
				u8 was = cur[cy * LIFE_COLS + cx];
				u8 now = next[cy * LIFE_COLS + cx];

				if (was == now)
					continue;
				ret = st7789_fill_rect(priv, cx * LIFE_CELL,
						       cy * LIFE_CELL,
						       LIFE_CELL, LIFE_CELL,
						       now ? 0x07E0 : 0x0000);
				if (ret) goto out;
			}
		}

		swap(cur, next);
		msleep(DEMO_LIFE_GEN_DELAY_MS);
	}

out:
	kfree(cur);
	kfree(next);
	return ret;
}

static int st7789_probe(struct spi_device *spi)
{
	struct st7789_priv *priv;
	int ret;

	pr_info("Now doing probe, spi %px\n", spi);

	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi_setup() failed: %d\n", ret);
		return ret;
	}

	dev_info(&spi->dev, "ST7789 probe: speed=%u Hz mode=0x%02x\n",
		 spi->max_speed_hz, spi->mode);

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
		dev_err(&spi->dev, "display init failed: %d\n", ret);
		return ret;
	}

	ret = st7789_demo(priv);
	if (ret) {
		dev_err(&spi->dev, "demo pattern failed: %d\n", ret);
		return ret;
	}

	dev_info(&spi->dev, "ST7789 240x240 initialized successfully\n");
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
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, st7789_of_match);

static const struct spi_device_id st7789_spi_ids[] = {
	{ "st7789", 0 },
	{ /* sentinel */ }
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
MODULE_DESCRIPTION("LKSS Lab 3: ST7789 SPI display driver - reference solution");
MODULE_LICENSE("GPL v2");
