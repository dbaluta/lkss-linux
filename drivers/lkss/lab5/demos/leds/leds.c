// SPDX-License-Identifier: GPL-2.0
/*
 * leds - LVGL warm-up sample #3
 *
 * A tiny LED control panel: three circles on screen mirror the three
 * LEDs on the daughter board.
 *
 *   SW1 = toggle red, SW2 = toggle green, SW3 = toggle blue,
 *   SW4 = all off
 *
 * Shows the two halves of the hackpad driver working together: button
 * *events* in (hal_button_pressed) and LED *state* out (hal_led).
 *
 * Needs st7789fb.ko and hackpad.ko.
 */
#include "hal.h"

static const uint32_t led_colors[HACKPAD_NUM_LEDS] = {
	0xFF3030,	/* red   */
	0x30FF30,	/* green */
	0x4060FF,	/* blue  */
};

static lv_obj_t *dots[HACKPAD_NUM_LEDS];
static bool led_on[HACKPAD_NUM_LEDS];

/* Bright circle when the LED is on, dark outline when off */
static void show_led(int i)
{
	lv_color_t c = lv_color_hex(led_colors[i]);

	lv_obj_set_style_bg_color(dots[i],
			led_on[i] ? c : lv_color_darken(c, LV_OPA_80), 0);
	lv_obj_set_style_border_color(dots[i], c, 0);
	hal_led(i, led_on[i]);
}

static void poll_buttons(lv_timer_t *t)
{
	LV_UNUSED(t);

	for (int i = 0; i < HACKPAD_NUM_LEDS; i++) {
		/* SW1/SW2/SW3 indexes match LED indexes */
		if (hal_button_pressed(i)) {
			led_on[i] = !led_on[i];
			show_led(i);
		}
	}

	if (hal_button_pressed(HACKPAD_BTN_SW4)) {
		for (int i = 0; i < HACKPAD_NUM_LEDS; i++) {
			led_on[i] = false;
			show_led(i);
		}
	}
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t *title = lv_label_create(scr);
	lv_label_set_text(title, "LED control");
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

	for (int i = 0; i < HACKPAD_NUM_LEDS; i++) {
		dots[i] = lv_obj_create(scr);
		lv_obj_set_size(dots[i], 56, 56);
		lv_obj_set_pos(dots[i], 16 + i * 76, 90);
		lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_border_width(dots[i], 3, 0);

		lv_obj_t *lbl = lv_label_create(scr);
		lv_label_set_text_fmt(lbl, "SW%d", i + 1);
		lv_obj_set_style_text_color(lbl, lv_color_hex(0x808080), 0);
		lv_obj_set_pos(lbl, 30 + i * 76, 152);

		show_led(i);
	}

	lv_obj_t *hint = lv_label_create(scr);
	lv_label_set_text(hint, "SW1-SW3 toggle, SW4 all off");
	lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), 0);
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -16);

	lv_timer_create(poll_buttons, 20, NULL);

	hal_run();
	return 0;
}
