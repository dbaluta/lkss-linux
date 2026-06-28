// SPDX-License-Identifier: GPL-2.0
/*
 * lkss_fb_demo_sol.c – LKSS Lab 3: framebuffer userspace demo reference solution
 *
 * Complete implementation of every TODO in lkss_fb_demo.c.
 *
 * Build:
 *   aarch64-linux-gnu-gcc -O2 -o lkss_fb_demo_sol lkss_fb_demo_sol.c
 *
 * Run (on board):
 *   insmod /tmp/lkss_st7789.ko
 *   ./lkss_fb_demo_sol /dev/fb0
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
	uint16_t *buf;
	int       width;
	int       height;
	int       stride;
	int       size;
};

/* ── Helpers ─────────────────────────────────────────────────────────────── */

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

/* ── TODO U1 (solved): Open device, query screen info, mmap ──────────────── */

static int fb_open(struct fb_ctx *ctx, const char *dev)
{
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	ctx->fd = open(dev, O_RDWR);
	if (ctx->fd < 0) {
		perror("open");
		return -1;
	}

	if (ioctl(ctx->fd, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("FBIOGET_VSCREENINFO");
		close(ctx->fd);
		return -1;
	}

	if (ioctl(ctx->fd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("FBIOGET_FSCREENINFO");
		close(ctx->fd);
		return -1;
	}

	ctx->width  = (int)vinfo.xres;
	ctx->height = (int)vinfo.yres;
	ctx->stride = (int)(finfo.line_length / 2);  /* bytes → pixels */
	ctx->size   = (int)finfo.smem_len;

	ctx->buf = mmap(NULL, ctx->size,
			PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
	if (ctx->buf == MAP_FAILED) {
		perror("mmap");
		close(ctx->fd);
		return -1;
	}

	return 0;
}

static void fb_close(struct fb_ctx *ctx)
{
	munmap(ctx->buf, ctx->size);
	close(ctx->fd);
}

/* ── TODO U2 (solved): Solid color fill ─────────────────────────────────── */

static void demo_solid_fill(struct fb_ctx *ctx, uint16_t color)
{
	fill_rect(ctx, 0, 0, ctx->width, ctx->height, color);
}

/* ── TODO U3 (solved): Chess board ──────────────────────────────────────── */

static void demo_chess(struct fb_ctx *ctx)
{
	uint16_t light = WHITE;
	uint16_t dark  = BLACK;
	int sq = ctx->width / 8;  /* square size: 30 px for a 240-pixel display */
	int row, col;

	for (row = 0; row < 8; row++) {
		for (col = 0; col < 8; col++) {
			uint16_t color = ((row + col) % 2 == 0) ? light : dark;
			fill_rect(ctx, col * sq, row * sq, sq, sq, color);
		}
	}
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

	printf("Chess board...\n");
	demo_chess(&ctx);
	fb_flush(&ctx);
	sleep(5);

	fb_close(&ctx);
	return 0;
}
