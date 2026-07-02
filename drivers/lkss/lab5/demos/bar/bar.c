// SPDX-License-Identifier: GPL-2.0
/*
 * bar - LVGL warm-up sample: a progress bar driven by buttons
 *
 * Hold SW2 to fill the bar, SW1 to drain it. The label always shows
 * the value and the LEDs act as a level meter (red at 1/3, +green at
 * 2/3, +blue when full).
 *
 * lv_bar is the widget behind every gauge-like display: CPU/memory in
 * sysmon, health/energy in a game, a progress screen...
 *
 * Needs st7789fb.ko and hackpad.ko.
 *
 * API used:
 *   lv_bar_create(parent)                    the widget
 *   lv_bar_set_range(bar, 0, 100)
 *   lv_bar_set_value(bar, v, LV_ANIM_OFF)
 *   lv_obj_set_style_bg_color(bar, c, LV_PART_MAIN)       trough color
 *   lv_obj_set_style_bg_color(bar, c, LV_PART_INDICATOR)  fill color
 *   hal_buttons()                            held buttons (level)
 */
#include "hal.h"

static lv_obj_t *bar;
static lv_obj_t *value_lbl;
static int value = 50;

static void poll_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	uint32_t btns = hal_buttons();
	int old = value;

	if (btns & (1 << HACKPAD_BTN_SW1))
		value--;
	if (btns & (1 << HACKPAD_BTN_SW2))
		value++;
	value = LV_CLAMP(0, value, 100);

	if (value == old)
		return;

	lv_bar_set_value(bar, value, LV_ANIM_OFF);
	lv_label_set_text_fmt(value_lbl, "%d %%", value);

	/* LED level meter */
	uint32_t mask = 0;
	if (value >= 33)
		mask |= 1 << HACKPAD_LED_RED;
	if (value >= 66)
		mask |= 1 << HACKPAD_LED_GREEN;
	if (value >= 100)
		mask |= 1 << HACKPAD_LED_BLUE;
	hal_leds(mask);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	bar = lv_bar_create(scr);
	lv_obj_set_size(bar, 200, 24);
	lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
	lv_bar_set_range(bar, 0, 100);
	lv_bar_set_value(bar, value, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(bar, lv_color_hex(0x202020), LV_PART_MAIN);
	lv_obj_set_style_bg_color(bar, lv_color_hex(0x00C0FF),
				  LV_PART_INDICATOR);

	value_lbl = lv_label_create(scr);
	lv_obj_set_style_text_font(value_lbl, &lv_font_montserrat_24, 0);
	lv_obj_set_style_text_color(value_lbl, lv_color_white(), 0);
	lv_obj_align(value_lbl, LV_ALIGN_CENTER, 0, -50);
	lv_label_set_text_fmt(value_lbl, "%d %%", value);

	lv_obj_t *hint = lv_label_create(scr);
	lv_label_set_text(hint, "SW1 drain - SW2 fill");
	lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), 0);
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);

	lv_timer_create(poll_tick, 30, NULL);

	hal_run();
	return 0;
}
