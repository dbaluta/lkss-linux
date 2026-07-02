// SPDX-License-Identifier: GPL-2.0
/*
 * jump - LVGL warm-up sample #4
 *
 * Four colored balls sit on the floor; each button kicks "its" ball
 * into the air and gravity brings it back down:
 *
 *   SW1 = red ball, SW2 = green ball, SW3 = blue ball, SW4 = yellow
 *
 * This is the smallest possible "game": a 16 ms lv_timer as game loop,
 * per-object physics (velocity + gravity), and edge-triggered button
 * input. Every game demo (pong, breakout...) is this pattern, scaled up.
 *
 * Needs st7789fb.ko and hackpad.ko.
 */
#include "hal.h"

#define NUM_BALLS  4
#define BALL_SIZE  32
#define FLOOR_Y    200			/* top of the floor line        */
#define GROUND     (FLOOR_Y - BALL_SIZE)	/* ball y when resting  */
#define JUMP_V     (-11)		/* kick velocity (px/frame)     */
#define GRAVITY    1			/* px/frame^2                   */

static const uint32_t ball_colors[NUM_BALLS] = {
	0xFF3030,	/* SW1: red    */
	0x30FF30,	/* SW2: green  */
	0x4060FF,	/* SW3: blue   */
	0xFFD000,	/* SW4: yellow */
};

static lv_obj_t *balls[NUM_BALLS];
static int y[NUM_BALLS];		/* ball top position */
static int vy[NUM_BALLS];		/* vertical velocity */

static void game_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	for (int i = 0; i < NUM_BALLS; i++) {
		bool on_ground = (y[i] == GROUND && vy[i] == 0);

		/* A press only kicks a resting ball - no double jumps */
		if (hal_button_pressed(i) && on_ground)
			vy[i] = JUMP_V;

		if (!on_ground) {
			y[i] += vy[i];
			vy[i] += GRAVITY;
			if (y[i] >= GROUND) {	/* landed */
				y[i] = GROUND;
				vy[i] = 0;
			}
			lv_obj_set_y(balls[i], y[i]);
		}

		/* Mirror the first three balls on the LEDs while airborne */
		if (i < HACKPAD_NUM_LEDS)
			hal_led(i, !on_ground);
	}
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* The floor */
	lv_obj_t *floor = lv_obj_create(scr);
	lv_obj_set_size(floor, 240, 4);
	lv_obj_set_pos(floor, 0, FLOOR_Y);
	lv_obj_set_style_bg_color(floor, lv_color_hex(0x808080), 0);
	lv_obj_set_style_border_width(floor, 0, 0);
	lv_obj_set_style_radius(floor, 0, 0);

	for (int i = 0; i < NUM_BALLS; i++) {
		int x = 12 + i * 56;

		y[i] = GROUND;
		balls[i] = lv_obj_create(scr);
		lv_obj_set_size(balls[i], BALL_SIZE, BALL_SIZE);
		lv_obj_set_pos(balls[i], x, y[i]);
		lv_obj_set_style_bg_color(balls[i],
					  lv_color_hex(ball_colors[i]), 0);
		lv_obj_set_style_border_width(balls[i], 0, 0);
		lv_obj_set_style_radius(balls[i], LV_RADIUS_CIRCLE, 0);

		lv_obj_t *lbl = lv_label_create(scr);
		lv_label_set_text_fmt(lbl, "SW%d", i + 1);
		lv_obj_set_style_text_color(lbl, lv_color_hex(0x808080), 0);
		lv_obj_set_pos(lbl, x + 2, FLOOR_Y + 12);
	}

	lv_obj_t *title = lv_label_create(scr);
	lv_label_set_text(title, "press a button,\njump its ball!");
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

	lv_timer_create(game_tick, 16, NULL);

	hal_run();
	return 0;
}
