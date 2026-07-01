// SPDX-License-Identifier: GPL-2.0
/*
 * st7789_scene.c - LKSS Lab 3, Exercise 11
 *
 * Userspace demo: draws four scenes to the ST7789 240x240 display
 * via the /dev/st7789fb miscdevice (mmap + ioctl flush).
 *
 * Build (see Makefile in this directory):
 *   make CROSS_COMPILE=aarch64-linux-gnu-
 *
 * Run on target:
 *   modprobe st7789.ko
 *   ./st7789_scene
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

#define W        240
#define H        240
#define FB_SIZE  (W * H * 2)

/* Must match the kernel ST7789FB_FLUSH definition */
#define ST7789FB_MAGIC  'F'
#define ST7789FB_FLUSH  _IO(ST7789FB_MAGIC, 0)

/* RGB565 color constants (native, converted to big-endian before writing) */
#define BLACK    0x0000
#define WHITE    0xFFFF
#define RED      0xF800
#define GREEN    0x07E0
#define BLUE     0x001F
#define YELLOW   0xFFE0
#define CYAN     0x07FF
#define MAGENTA  0xF81F

/*
 * ST7789 expects pixels big-endian over SPI.  The kernel flush copies the
 * framebuffer bytes verbatim, so userspace must store them in big-endian.
 * On the little-endian i.MX93 a byte-swap is required for each pixel value.
 */
static inline uint16_t to_be16(uint16_t c)
{
	return (uint16_t)((c >> 8) | (c << 8));
}

/* ---- Framebuffer drawing helpers ---------------------------------------- */

static void fb_fill(uint16_t *fb, uint16_t color)
{
	uint16_t c = to_be16(color);
	int i;
	for (i = 0; i < W * H; i++)
		fb[i] = c;
}

static void fb_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color)
{
	uint16_t c = to_be16(color);
	int r, col;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > W) w = W - x;
	if (y + h > H) h = H - y;
	if (w <= 0 || h <= 0) return;

	for (r = y; r < y + h; r++)
		for (col = x; col < x + w; col++)
			fb[r * W + col] = c;
}

static void fb_pixel(uint16_t *fb, int x, int y, uint16_t color)
{
	if (x >= 0 && x < W && y >= 0 && y < H)
		fb[y * W + x] = to_be16(color);
}

/* ---- Scenes ------------------------------------------------------------- */

static void scene_color_bars(uint16_t *fb)
{
	static const uint16_t bars[] = {
		RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BLACK
	};
	int n = sizeof(bars) / sizeof(bars[0]);
	int bw = W / n;
	int i;

	fb_fill(fb, BLACK);
	for (i = 0; i < n; i++)
		fb_rect(fb, i * bw, 0, bw, H, bars[i]);
}

static void scene_gradient(uint16_t *fb)
{
	int x, y;
	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			uint8_t r = (uint8_t)((x * 31) / (W - 1));
			uint8_t g = (uint8_t)((y * 63) / (H - 1));
			uint16_t px = (uint16_t)((r << 11) | (g << 5));
			fb[y * W + x] = to_be16(px);
		}
	}
}

static void scene_checkerboard(uint16_t *fb)
{
	int x, y;
	fb_fill(fb, BLACK);
	for (y = 0; y < H; y += 20)
		for (x = 0; x < W; x += 20)
			if (((x / 20) + (y / 20)) % 2 == 0)
				fb_rect(fb, x, y, 20, 20, WHITE);
}

static void scene_bordered_dots(uint16_t *fb)
{
	int x, y;

	fb_fill(fb, BLACK);
	fb_rect(fb,   0,   0, W,  8, RED);
	fb_rect(fb,   0, H-8, W,  8, RED);
	fb_rect(fb,   0,   0, 8,  H, GREEN);
	fb_rect(fb, W-8,   0, 8,  H, BLUE);

	for (y = 20; y < H - 20; y += 20)
		for (x = 20; x < W - 20; x += 20)
			fb_pixel(fb, x, y, WHITE);
}

/* ---- Main --------------------------------------------------------------- */

int main(void)
{
	int fd;
	uint16_t *fb;
	int i;

	static void (*scenes[])(uint16_t *) = {
		scene_color_bars,
		scene_gradient,
		scene_checkerboard,
		scene_bordered_dots,
	};
	static const char *names[] = {
		"color bars",
		"RGB gradient",
		"checkerboard",
		"bordered dot grid",
	};
	int nscenes = (int)(sizeof(scenes) / sizeof(scenes[0]));

	fd = open("/dev/st7789fb", O_RDWR);
	if (fd < 0) {
		perror("open /dev/st7789fb");
		return 1;
	}

	fb = mmap(NULL, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return 1;
	}

	for (i = 0; i < nscenes; i++) {
		printf("Scene %d/%d: %s\n", i + 1, nscenes, names[i]);
		scenes[i](fb);
		if (ioctl(fd, ST7789FB_FLUSH) < 0) {
			perror("ioctl ST7789FB_FLUSH");
			break;
		}
		sleep(2);
	}

	fb_fill(fb, BLACK);
	ioctl(fd, ST7789FB_FLUSH);

	munmap(fb, FB_SIZE);
	close(fd);
	return 0;
}
