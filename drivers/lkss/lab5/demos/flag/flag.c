// SPDX-License-Identifier: GPL-2.0
/*
 * flag - LVGL warm-up sample #2
 *
 * Paints the screen with simple full-height rectangles, one per
 * palette color - vertical stripes, like an oversized six-color flag.
 *
 * Shows how to create, size and position plain lv_obj rectangles - the
 * same technique the snake, breakout and tetris demos are built on.
 *
 * Needs only st7789fb.ko (no buttons required).
 */
#include "hal.h"

static const uint32_t palette[] = {
	0xE81B23,	/* red    */
	0xF68A1E,	/* orange */
	0xFFD500,	/* yellow */
	0x009E49,	/* green  */
	0x0066B3,	/* blue   */
	0x732982,	/* purple */
};

#define NCOLORS (sizeof(palette) / sizeof(palette[0]))
#define STRIPE  (240 / NCOLORS)		/* 6 stripes of 40 px */

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	for (unsigned int i = 0; i < NCOLORS; i++) {
		lv_obj_t *rect = lv_obj_create(scr);

		lv_obj_set_size(rect, STRIPE, 240);
		lv_obj_set_pos(rect, i * STRIPE, 0);
		lv_obj_set_style_bg_color(rect, lv_color_hex(palette[i]), 0);
		lv_obj_set_style_border_width(rect, 0, 0);
		lv_obj_set_style_radius(rect, 0, 0);
	}

	hal_run();
	return 0;
}
