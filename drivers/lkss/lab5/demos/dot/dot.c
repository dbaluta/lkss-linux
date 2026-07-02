// SPDX-License-Identifier: GPL-2.0
/*
 * dot - LVGL warm-up sample: move a dot on a grid
 *
 * A square dot moves one grid cell per button press:
 *   SW1 = left, SW2 = right, SW3 = up, SW4 = down
 * and wraps around the screen edges. This is exactly how the snake
 * moves - a position in *cells*, multiplied by the cell size only
 * when drawing.
 *
 * Needs st7789fb.ko and hackpad.ko.
 *
 * API used:
 *   hal_button_pressed(btn)     true once per press (edge) - one step
 *                               per press, no auto-repeat
 *   lv_obj_set_pos(obj, x, y)   pixel position = cell * CELL
 */
#include "hal.h"

#define CELL  16
#define GRID  15		/* 15 x 16 = 240 px */

static lv_obj_t *dot;
static lv_obj_t *pos_lbl;
static int cx = GRID / 2, cy = GRID / 2;	/* position in cells */

static void redraw(void)
{
	lv_obj_set_pos(dot, cx * CELL + 2, cy * CELL + 2);
	lv_label_set_text_fmt(pos_lbl, "cell %d,%d", cx, cy);
}

static void poll_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	bool moved = false;

	if (hal_button_pressed(HACKPAD_BTN_SW1)) { cx--; moved = true; }
	if (hal_button_pressed(HACKPAD_BTN_SW2)) { cx++; moved = true; }
	if (hal_button_pressed(HACKPAD_BTN_SW3)) { cy--; moved = true; }
	if (hal_button_pressed(HACKPAD_BTN_SW4)) { cy++; moved = true; }

	if (moved) {
		/* Wrap around the edges ((x + GRID) keeps -1 positive) */
		cx = (cx + GRID) % GRID;
		cy = (cy + GRID) % GRID;
		redraw();
	}
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	dot = lv_obj_create(scr);
	lv_obj_set_size(dot, CELL - 4, CELL - 4);
	lv_obj_set_style_bg_color(dot, lv_color_hex(0x00FF00), 0);
	lv_obj_set_style_border_width(dot, 0, 0);
	lv_obj_set_style_radius(dot, 2, 0);

	pos_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(pos_lbl, lv_color_hex(0x808080), 0);
	lv_obj_align(pos_lbl, LV_ALIGN_TOP_RIGHT, -4, 2);

	redraw();
	lv_timer_create(poll_tick, 20, NULL);

	hal_run();
	return 0;
}
