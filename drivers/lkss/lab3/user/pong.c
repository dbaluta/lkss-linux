// SPDX-License-Identifier: GPL-2.0
/*
 * pong.c - LKSS Lab 3, Exercise 13
 *
 * Two-player Pong on the ST7789 240x240 display.
 *
 * Kernel interfaces used:
 *   /dev/st7789fb      - mmap framebuffer + ioctl ST7789FB_FLUSH (exercise 10)
 *   /sys/.../button0-3 - push-button states from platform_gpio (exercise 12)
 *
 * Controls:
 *   Button 0 - Player 1 paddle UP
 *   Button 1 - Player 1 paddle DOWN
 *   Button 2 - Player 2 paddle UP
 *   Button 3 - Player 2 paddle DOWN
 *
 * Build:
 *   make CROSS_COMPILE=aarch64-linux-gnu-
 *
 * Run on target:
 *   modprobe st7789.ko
 *   modprobe platform_gpio.ko
 *   ./pong
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

/* Display geometry */
#define W  240
#define H  240
#define FB_SIZE (W * H * 2)

/* ioctl - must match kernel definition */
#define ST7789FB_MAGIC  'F'
#define ST7789FB_FLUSH  _IO(ST7789FB_MAGIC, 0)

/* RGB565 colors */
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF

/* Game constants */
#define PADDLE_W      8
#define PADDLE_H     40
#define BALL_SZ       6
#define PADDLE_SPEED  4
#define BALL_DX       3
#define BALL_DY       2
#define SCORE_AREA   16   /* height reserved at the top for scores */
#define MAX_SCORE     9

/*
 * Sysfs paths for buttons - adjust if kobject_get_path() printed a different
 * path when you loaded platform_gpio.ko (check dmesg after modprobe).
 */
static const char * const BTN[4] = {
	"/sys/devices/platform/lkss-gpio/button0",
	"/sys/devices/platform/lkss-gpio/button1",
	"/sys/devices/platform/lkss-gpio/button2",
	"/sys/devices/platform/lkss-gpio/button3",
};

/* ---- Framebuffer helpers ------------------------------------------------ */

static uint16_t *g_fb;

static inline uint16_t be16(uint16_t c)
{
	return (uint16_t)((c >> 8) | (c << 8));
}

static void rect(int x, int y, int w, int h, uint16_t c)
{
	uint16_t bc = be16(c);
	int r, col;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > W) w = W - x;
	if (y + h > H) h = H - y;
	if (w <= 0 || h <= 0) return;

	for (r = y; r < y + h; r++)
		for (col = x; col < x + w; col++)
			g_fb[r * W + col] = bc;
}

static void clr(uint16_t c)
{
	uint16_t bc = be16(c);
	int i;
	for (i = 0; i < W * H; i++)
		g_fb[i] = bc;
}

/* Tiny 3x5 pixel font for score digits 0-9 */
static const uint8_t FONT[10][5] = {
	{ 0x7, 0x5, 0x5, 0x5, 0x7 }, /* 0 */
	{ 0x2, 0x2, 0x2, 0x2, 0x2 }, /* 1 */
	{ 0x7, 0x1, 0x7, 0x4, 0x7 }, /* 2 */
	{ 0x7, 0x1, 0x3, 0x1, 0x7 }, /* 3 */
	{ 0x5, 0x5, 0x7, 0x1, 0x1 }, /* 4 */
	{ 0x7, 0x4, 0x7, 0x1, 0x7 }, /* 5 */
	{ 0x7, 0x4, 0x7, 0x5, 0x7 }, /* 6 */
	{ 0x7, 0x1, 0x1, 0x1, 0x1 }, /* 7 */
	{ 0x7, 0x5, 0x7, 0x5, 0x7 }, /* 8 */
	{ 0x7, 0x5, 0x7, 0x1, 0x7 }, /* 9 */
};

static void draw_digit(int ox, int oy, int d, uint16_t c)
{
	int row, col;
	for (row = 0; row < 5; row++)
		for (col = 0; col < 3; col++)
			if (FONT[d % 10][row] & (4 >> col))
				rect(ox + col * 2, oy + row * 2, 2, 2, c);
}

/* ---- Button input ------------------------------------------------------- */

static int btn(int idx)
{
	char buf[4];
	int fd, n;

	fd = open(BTN[idx], O_RDONLY);
	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	return buf[0] == '1';
}

/* ---- Rendering ---------------------------------------------------------- */

static void render(int p1y, int p2y, int bx, int by, int s1, int s2)
{
	int y;

	clr(BLACK);

	/* Score area separator */
	rect(0, SCORE_AREA - 1, W, 1, WHITE);

	/* Centre dashed line */
	for (y = SCORE_AREA; y < H; y += 10)
		rect(W / 2 - 1, y, 2, 5, WHITE);

	/* Paddles */
	rect(8,           p1y, PADDLE_W, PADDLE_H, GREEN);
	rect(W-8-PADDLE_W, p2y, PADDLE_W, PADDLE_H, RED);

	/* Ball */
	rect(bx, by, BALL_SZ, BALL_SZ, YELLOW);

	/* Scores */
	draw_digit(W / 4 - 3,     2, s1, WHITE);
	draw_digit(3 * W / 4 - 3, 2, s2, WHITE);
}

/* ---- Nanosleep wrapper -------------------------------------------------- */

static void nsleep(long ns)
{
	struct timespec ts = { 0, ns };
	nanosleep(&ts, NULL);
}

/* ---- Main --------------------------------------------------------------- */

int main(void)
{
	int fb_fd;
	int p1y, p2y, bx, by, bdx, bdy, s1, s2;
	/* left paddle X, right paddle X */
	const int LX = 8;
	const int RX = W - 8 - PADDLE_W;

	fb_fd = open("/dev/st7789fb", O_RDWR);
	if (fb_fd < 0) { perror("open /dev/st7789fb"); return 1; }

	g_fb = mmap(NULL, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (g_fb == MAP_FAILED) { perror("mmap"); close(fb_fd); return 1; }

	/* Initial state */
	p1y = H / 2 - PADDLE_H / 2;
	p2y = H / 2 - PADDLE_H / 2;
	bx  = W / 2 - BALL_SZ / 2;
	by  = H / 2 - BALL_SZ / 2;
	bdx = BALL_DX;
	bdy = BALL_DY;
	s1  = 0;
	s2  = 0;

	printf("Pong!  B0/B1 = P1 up/down  B2/B3 = P2 up/down\n");
	printf("First to %d wins.  Press Ctrl-C to quit.\n\n", MAX_SCORE);

	while (s1 < MAX_SCORE && s2 < MAX_SCORE) {

		/* -- Input -------------------------------------------------- */
		if (btn(0) && p1y > SCORE_AREA)           p1y -= PADDLE_SPEED;
		if (btn(1) && p1y + PADDLE_H < H)         p1y += PADDLE_SPEED;
		if (btn(2) && p2y > SCORE_AREA)           p2y -= PADDLE_SPEED;
		if (btn(3) && p2y + PADDLE_H < H)         p2y += PADDLE_SPEED;

		/* -- Ball physics ------------------------------------------- */
		bx += bdx;
		by += bdy;

		/* Top / bottom wall */
		if (by <= SCORE_AREA) { by = SCORE_AREA; bdy = -bdy; }
		if (by + BALL_SZ >= H) { by = H - BALL_SZ; bdy = -bdy; }

		/* Left paddle */
		if (bx <= LX + PADDLE_W && bx >= LX &&
		    by + BALL_SZ >= p1y && by <= p1y + PADDLE_H) {
			bx  = LX + PADDLE_W;
			bdx = -bdx;
		}

		/* Right paddle */
		if (bx + BALL_SZ >= RX && bx + BALL_SZ <= RX + PADDLE_W &&
		    by + BALL_SZ >= p2y && by <= p2y + PADDLE_H) {
			bx  = RX - BALL_SZ;
			bdx = -bdx;
		}

		/* -- Scoring ------------------------------------------------ */
		if (bx + BALL_SZ < 0) {
			s2++;
			printf("P2 scores! %d-%d\n", s1, s2);
			bx = W / 2 - BALL_SZ / 2;
			by = H / 2 - BALL_SZ / 2;
			bdx = BALL_DX;
			nsleep(500000000L);
		}
		if (bx > W) {
			s1++;
			printf("P1 scores! %d-%d\n", s1, s2);
			bx = W / 2 - BALL_SZ / 2;
			by = H / 2 - BALL_SZ / 2;
			bdx = -BALL_DX;
			nsleep(500000000L);
		}

		/* -- Render + flush ----------------------------------------- */
		render(p1y, p2y, bx, by, s1, s2);
		ioctl(fb_fd, ST7789FB_FLUSH);

		nsleep(33333333L); /* ~30 FPS */
	}

	/* Game over screen */
	clr(BLACK);
	if (s1 >= MAX_SCORE) {
		printf("Player 1 wins! %d-%d\n", s1, s2);
		draw_digit(W / 2 - 10, H / 2 - 5, s1, GREEN);
	} else {
		printf("Player 2 wins! %d-%d\n", s1, s2);
		draw_digit(W / 2 + 4,  H / 2 - 5, s2, RED);
	}
	ioctl(fb_fd, ST7789FB_FLUSH);
	sleep(3);

	munmap(g_fb, FB_SIZE);
	close(fb_fd);
	return 0;
}
