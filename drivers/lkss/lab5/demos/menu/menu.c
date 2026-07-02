// SPDX-License-Identifier: GPL-2.0
/*
 * menu - LVGL warm-up sample: multiple screens, button navigation
 *
 * Three "screens" and a title bar; SW1/SW2 flip between them:
 *   SW1 = previous screen, SW2 = next screen
 *
 * The trick: every screen is a full-size container created up front,
 * and "navigation" is just hiding all of them except one with
 * LV_OBJ_FLAG_HIDDEN. No allocation, no teardown, instant switching.
 * The watch faces and the weather station menus work exactly like
 * this.
 *
 * Needs st7789fb.ko and hackpad.ko.
 *
 * API used:
 *   lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)      hide a screen
 *   lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN)   show a screen
 *   hal_button_pressed(btn)                       edge-triggered press
 *   (cur + 1) % N and (cur + N - 1) % N           wrap-around index
 */
#include "hal.h"

#define NUM_SCREENS 3

static const char *names[NUM_SCREENS] = { "HOME", "STATS", "ABOUT" };
static const uint32_t tints[NUM_SCREENS] = { 0x103020, 0x102030, 0x301020 };

static lv_obj_t *screens[NUM_SCREENS];
static lv_obj_t *title_lbl;
static int cur;

static void show_screen(int idx)
{
	for (int i = 0; i < NUM_SCREENS; i++) {
		if (i == idx)
			lv_obj_remove_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
		else
			lv_obj_add_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
	}
	lv_label_set_text_fmt(title_lbl, "< %s >", names[idx]);
	cur = idx;
}

static void nav_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	if (hal_button_pressed(HACKPAD_BTN_SW2))
		show_screen((cur + 1) % NUM_SCREENS);
	if (hal_button_pressed(HACKPAD_BTN_SW1))
		show_screen((cur + NUM_SCREENS - 1) % NUM_SCREENS);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	title_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(title_lbl, lv_color_white(), 0);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_18, 0);
	lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 4);

	for (int i = 0; i < NUM_SCREENS; i++) {
		lv_obj_t *s = lv_obj_create(scr);

		lv_obj_set_size(s, 240, 216);
		lv_obj_set_pos(s, 0, 24);
		lv_obj_set_style_bg_color(s, lv_color_hex(tints[i]), 0);
		lv_obj_set_style_border_width(s, 0, 0);
		lv_obj_set_style_radius(s, 0, 0);
		lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);

		lv_obj_t *lbl = lv_label_create(s);
		lv_label_set_text_fmt(lbl, "screen %d\n%s", i + 1, names[i]);
		lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
		lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
		lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_center(lbl);

		screens[i] = s;
	}

	show_screen(0);
	lv_timer_create(nav_tick, 20, NULL);

	hal_run();
	return 0;
}
