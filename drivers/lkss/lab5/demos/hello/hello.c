// SPDX-License-Identifier: GPL-2.0
/*
 * hello - LVGL warm-up sample: text and labels
 *
 * The very first thing to try: put text on the screen. Shows the three
 * things you do with every label: create it, style it (font + color),
 * and position it. The counter label also shows how to update text
 * from a timer with printf-style formatting.
 *
 * Needs only st7789fb.ko (no buttons required).
 *
 * API used:
 *   lv_label_create(parent)                        new text object
 *   lv_label_set_text(lbl, "...")                  static text
 *   lv_label_set_text_fmt(lbl, "%d", v)            printf-style text
 *   lv_obj_set_style_text_font(lbl, &font, 0)      lv_font_montserrat_N
 *   lv_obj_set_style_text_color(lbl, color, 0)     lv_color_hex(0xRRGGBB)
 *   lv_obj_align(lbl, LV_ALIGN_*, dx, dy)          position vs. parent
 */
#include "hal.h"

static lv_obj_t *counter_lbl;

static void count_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	static int seconds;

	lv_label_set_text_fmt(counter_lbl, "alive for %d s", seconds++);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* Big title */
	lv_obj_t *title = lv_label_create(scr);
	lv_label_set_text(title, "Hello LKSS!");
	lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

	/* Smaller subtitle in another color */
	lv_obj_t *sub = lv_label_create(scr);
	lv_label_set_text(sub, "drawn by LVGL on /dev/fb0");
	lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(sub, lv_color_hex(0x00C0FF), 0);
	lv_obj_align(sub, LV_ALIGN_CENTER, 0, 0);

	/* A label a timer updates once per second */
	counter_lbl = lv_label_create(scr);
	lv_obj_set_style_text_font(counter_lbl, &lv_font_montserrat_18, 0);
	lv_obj_set_style_text_color(counter_lbl, lv_color_hex(0xFFA000), 0);
	lv_obj_align(counter_lbl, LV_ALIGN_BOTTOM_MID, 0, -30);
	lv_label_set_text(counter_lbl, "alive for 0 s");

	lv_timer_create(count_tick, 1000, NULL);

	hal_run();
	return 0;
}
