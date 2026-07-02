// SPDX-License-Identifier: GPL-2.0
/*
 * paddle - LVGL warm-up sample: hold a button to move a paddle
 *
 * A paddle at the bottom of the screen slides left/right *while* a
 * button is held down:
 *   SW1 = move left, SW2 = move right
 *
 * This is the other half of game input (dot showed the edge/press
 * kind): a 16 ms timer polls the *level* of the buttons, so movement
 * is smooth and continues as long as you hold. Pong and breakout use
 * exactly this.
 *
 * Needs st7789fb.ko and hackpad.ko.
 *
 * API used:
 *   hal_buttons()               bitmask of buttons held *right now*;
 *                               test with (btns & (1 << HACKPAD_BTN_SW1))
 *   LV_CLAMP(min, v, max)       keep the paddle on screen
 *   lv_obj_set_x(obj, x)        move only horizontally
 */
#include "hal.h"

#define SCREEN    240
#define PADDLE_W  44
#define PADDLE_H  8
#define PADDLE_Y  (SCREEN - 20)
#define SPEED     4		/* pixels per 16 ms frame */

static lv_obj_t *paddle;
static int paddle_x = SCREEN / 2;	/* paddle center */

static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	uint32_t btns = hal_buttons();

	if (btns & (1 << HACKPAD_BTN_SW1))
		paddle_x -= SPEED;
	if (btns & (1 << HACKPAD_BTN_SW2))
		paddle_x += SPEED;

	paddle_x = LV_CLAMP(PADDLE_W / 2, paddle_x, SCREEN - PADDLE_W / 2);
	lv_obj_set_x(paddle, paddle_x - PADDLE_W / 2);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	paddle = lv_obj_create(scr);
	lv_obj_set_size(paddle, PADDLE_W, PADDLE_H);
	lv_obj_set_pos(paddle, paddle_x - PADDLE_W / 2, PADDLE_Y);
	lv_obj_set_style_bg_color(paddle, lv_color_white(), 0);
	lv_obj_set_style_border_width(paddle, 0, 0);
	lv_obj_set_style_radius(paddle, 3, 0);

	lv_obj_t *hint = lv_label_create(scr);
	lv_label_set_text(hint, "hold SW1 / SW2");
	lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), 0);
	lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);

	lv_timer_create(game_tick, 16, NULL);

	hal_run();
	return 0;
}
