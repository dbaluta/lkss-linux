// SPDX-License-Identifier: GPL-2.0
/*
 * bounce - LVGL warm-up sample: a ball bouncing off the walls
 *
 * No input at all: a ball moves by its velocity every frame and the
 * velocity component flips sign when it hits an edge. This position +
 * velocity + "negate on collision" trio is the entire physics of pong
 * and breakout - combine it with the 'paddle' sample and you are
 * halfway to a game.
 *
 * Needs only st7789fb.ko (no buttons required).
 *
 * The pattern, in three lines:
 *   x += vx;                              move
 *   if (x <= 0 || x >= max) vx = -vx;     bounce
 *   lv_obj_set_pos(ball, x, y);           draw
 */
#include "hal.h"

#define SCREEN     240
#define BALL_SIZE  12

static lv_obj_t *ball;
static int x = 30, y = 60;	/* ball top-left corner */
static int vx = 3, vy = 2;	/* velocity, pixels per frame */

static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	x += vx;
	y += vy;

	if (x <= 0 || x >= SCREEN - BALL_SIZE)
		vx = -vx;
	if (y <= 0 || y >= SCREEN - BALL_SIZE)
		vy = -vy;

	lv_obj_set_pos(ball, x, y);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	ball = lv_obj_create(scr);
	lv_obj_set_size(ball, BALL_SIZE, BALL_SIZE);
	lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFF00), 0);
	lv_obj_set_style_border_width(ball, 0, 0);
	lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);

	lv_timer_create(game_tick, 16, NULL);	/* ~60 fps */

	hal_run();
	return 0;
}
