// SPDX-License-Identifier: GPL-2.0
/*
 * blink - LVGL warm-up sample: buttons change the screen + blink a LED
 *
 * Press a button and two things happen at once:
 *   - the screen background changes to that button's color
 *   - the matching LED blinks three times
 *
 *   SW1 = red, SW2 = green, SW3 = blue, SW4 = back to black / LEDs off
 *
 * Shows the screen and the LEDs reacting *together* to one input -
 * the pattern for any "feedback" effect (score flash, life lost,
 * level up...).
 *
 * Needs st7789fb.ko and hackpad.ko.
 *
 * API used:
 *   hal_button_pressed(btn)     true once per press (edge)
 *   hal_led(led, on)            one LED on/off
 *   lv_obj_set_style_bg_color(lv_screen_active(), color, 0)
 *
 * The blink itself is a mini-pattern worth stealing: instead of
 * sleeping (which would freeze the UI!), a fast timer decrements a
 * counter and toggles the LED on each tick.
 */
#include "hal.h"

static const uint32_t colors[3] = { 0xC00000, 0x00A000, 0x0040C0 };

static lv_obj_t *msg_lbl;
static int blink_led = -1;	/* which LED is blinking, -1 = none */
static int blink_ticks;		/* toggles left (2 per blink) */

/* Every 20 ms: react to presses */
static void poll_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	for (int i = 0; i < 3; i++) {
		/* SW1/SW2/SW3 indexes match color/LED indexes */
		if (!hal_button_pressed(i))
			continue;

		lv_obj_set_style_bg_color(lv_screen_active(),
					  lv_color_hex(colors[i]), 0);
		lv_label_set_text_fmt(msg_lbl, "SW%d!", i + 1);

		/* Start (or restart) the blink: 3 blinks = 6 toggles */
		hal_leds(0);
		blink_led = i;
		blink_ticks = 6;
	}

	if (hal_button_pressed(HACKPAD_BTN_SW4)) {
		lv_obj_set_style_bg_color(lv_screen_active(),
					  lv_color_black(), 0);
		lv_label_set_text(msg_lbl, "press SW1 / SW2 / SW3");
		blink_led = -1;
		hal_leds(0);
	}
}

/* Every 150 ms: advance the blink without ever sleeping */
static void blink_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	if (blink_led < 0 || blink_ticks == 0)
		return;

	/* Odd ticks = LED on, even ticks = LED off */
	hal_led(blink_led, blink_ticks % 2);
	blink_ticks--;
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	msg_lbl = lv_label_create(scr);
	lv_label_set_text(msg_lbl, "press SW1 / SW2 / SW3");
	lv_obj_set_style_text_color(msg_lbl, lv_color_white(), 0);
	lv_obj_set_style_text_font(msg_lbl, &lv_font_montserrat_24, 0);
	lv_obj_center(msg_lbl);

	lv_timer_create(poll_tick, 20, NULL);
	lv_timer_create(blink_tick, 150, NULL);

	hal_run();
	return 0;
}
