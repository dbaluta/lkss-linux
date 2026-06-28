// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_fb_demo.c – LKSS Lab 3: framebuffer userspace demo skeleton
 *
 * Prerequisite: lkss_st7789.ko loaded on the board (exercises 10 + 11 done).
 *
 * Exercise map:
 *   TODO U1  fb_open()          Open /dev/fb0, query screen info, mmap
 *   TODO U2  demo_solid_fill()  Fill the entire screen with one color
 *   TODO U3  demo_chess()       Draw an 8×8 chess board (30×30 px squares)
 *
 * Build (on host, cross-compile):
 *   aarch64-linux-gnu-gcc -O2 -o lkss_fb_demo lkss_fb_demo.c
 *
 * Run (on board):
 *   insmod /tmp/lkss_st7789.ko
 *   ./lkss_fb_demo /dev/fb0
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* RGB565 color constants (little-endian, native CPU format) */
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F

struct fb_ctx {
	int       fd;
	uint16_t *buf;      /* mmap'd framebuffer, one uint16_t per pixel */
	int       width;    /* pixels per row                              */
	int       height;   /* number of rows                              */
	int       stride;   /* pixels per row including any padding        */
	int       size;     /* total mapped size in bytes                  */
};

/* ── Helpers (provided) ──────────────────────────────────────────────────── */

static void set_pixel(struct fb_ctx *ctx, int x, int y, uint16_t color)
{
	if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height)
		return;
	ctx->buf[y * ctx->stride + x] = color;
}

static void fill_rect(struct fb_ctx *ctx,
		      int x, int y, int w, int h, uint16_t color)
{
	int dx, dy;
	for (dy = 0; dy < h; dy++)
		for (dx = 0; dx < w; dx++)
			set_pixel(ctx, x + dx, y + dy, color);
}

/*
 * fb_flush — push the mmap'd framebuffer to the display.
 *
 * Calls ioctl(FBIOPAN_DISPLAY) which triggers the driver's fb_pan_display
 * callback.  That callback acquires the SPI mutex and flushes all pixels.
 * Without this call the display does not update.
 */
static void fb_flush(struct fb_ctx *ctx)
{
	struct fb_var_screeninfo vinfo = {0};
	vinfo.xres         = (uint32_t)ctx->width;
	vinfo.yres         = (uint32_t)ctx->height;
	vinfo.xres_virtual = (uint32_t)ctx->width;
	vinfo.yres_virtual = (uint32_t)ctx->height;
	vinfo.bits_per_pixel = 16;
	ioctl(ctx->fd, FBIOPAN_DISPLAY, &vinfo);
}

/* ── TODO U1: Open device, query screen info, mmap framebuffer ──────────────
 *
 * Steps:
 *   1. open(dev, O_RDWR) – open the framebuffer device
 *   2. ioctl(ctx->fd, FBIOGET_VSCREENINFO, &vinfo) – get width, height, bpp
 *   3. ioctl(ctx->fd, FBIOGET_FSCREENINFO, &finfo) – get line_length, smem_len
 *   4. Populate ctx->width, ctx->height, ctx->size
 *      ctx->stride = finfo.line_length / 2  (pixels per row, not bytes)
 *   5. mmap(NULL, ctx->size, PROT_READ|PROT_WRITE, MAP_SHARED, ctx->fd, 0)
 *      – store result in ctx->buf; check for MAP_FAILED
 */
static int fb_open(struct fb_ctx *ctx, const char *dev)
{
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	/* TODO U1.1: open the device */
	ctx->fd = -1;
	if (ctx->fd < 0) {
		perror("open");
		return -1;
	}

	/* TODO U1.2: ioctl FBIOGET_VSCREENINFO */

	/* TODO U1.3: ioctl FBIOGET_FSCREENINFO */

	/* TODO U1.4: populate ctx fields */
	ctx->width  = 0;  /* vinfo.xres */
	ctx->height = 0;  /* vinfo.yres */
	ctx->stride = 0;  /* finfo.line_length / 2 */
	ctx->size   = 0;  /* finfo.smem_len */

	/* TODO U1.5: mmap the framebuffer */
	ctx->buf = MAP_FAILED;
	if (ctx->buf == MAP_FAILED) {
		perror("mmap");
		close(ctx->fd);
		return -1;
	}

	(void)vinfo; (void)finfo;  /* remove once TODO U1 is implemented */
	return 0;
}

static void fb_close(struct fb_ctx *ctx)
{
	munmap(ctx->buf, ctx->size);
	close(ctx->fd);
}

/* ── TODO U2: Solid color fill ───────────────────────────────────────────────
 *
 * Fill the entire screen with 'color'.
 * Hint: use fill_rect() with the full screen dimensions.
 */
static void demo_solid_fill(struct fb_ctx *ctx, uint16_t color)
{
	/* TODO U2 */
}

/* ── TODO U3: Chess board ────────────────────────────────────────────────────
 *
 * The ST7789 display is 240×240 pixels.  Divide it into an 8×8 grid of
 * 30×30-pixel squares (8 × 30 = 240).
 *
 * Rules:
 *   - The top-left square (row=0, col=0) is the light color.
 *   - A square is light if (row + col) is even, dark if (row + col) is odd.
 *   - The square at chess position (row, col) starts at pixel (col*sq, row*sq).
 *
 * Steps:
 *   int sq = ctx->width / 8;    // square size in pixels
 *   for each row 0..7:
 *     for each col 0..7:
 *       color = (row + col) % 2 == 0 ? light : dark
 *       fill_rect(ctx, col*sq, row*sq, sq, sq, color)
 *
 * Try different color pairs:
 *   Classic chess:  light = WHITE,  dark = BLACK
 *   Blue/orange:    light = CYAN,   dark = 0xD340
 */
static void demo_chess(struct fb_ctx *ctx)
{
	uint16_t light = WHITE;
	uint16_t dark  = BLACK;
	/* TODO U3 */
	(void)light; (void)dark;
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	struct fb_ctx ctx;
	const char *dev = (argc > 1) ? argv[1] : "/dev/fb0";

	if (fb_open(&ctx, dev) < 0)
		return 1;

	printf("Display: %dx%d, stride=%d px, buffer=%d bytes\n",
	       ctx.width, ctx.height, ctx.stride, ctx.size);

	/* Ex-13: solid color fill */
	printf("Solid red...\n");
	demo_solid_fill(&ctx, RED);
	fb_flush(&ctx);
	sleep(1);

	printf("Solid green...\n");
	demo_solid_fill(&ctx, GREEN);
	fb_flush(&ctx);
	sleep(1);

	printf("Solid blue...\n");
	demo_solid_fill(&ctx, BLUE);
	fb_flush(&ctx);
	sleep(1);

	/* Ex-14: chess board */
	printf("Chess board...\n");
	demo_chess(&ctx);
	fb_flush(&ctx);
	sleep(5);

	fb_close(&ctx);
	return 0;
}
