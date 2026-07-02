// SPDX-License-Identifier: GPL-2.0
/*
 * watch - a wristwatch (PROJECT SKELETON - fill in the TODOs)
 *
 * Goal: a smartwatch with three faces, fed by the system clock and
 * your Day 4 BMP280 driver:
 *   face 0: big digital clock + date + temperature
 *   face 1: seconds arc around the time
 *   face 2: temperature/pressure "complication" face
 *
 * Controls: SW1 = previous face, SW2 = next face
 *
 * The three faces and all their labels/arc are already created, and
 * clock_tick() already runs 10 times per second with the current time
 * decoded in 'tm'. You implement:
 *
 *   TODO 1: switch faces with SW1/SW2
 *   TODO 2: face 0 - time, date and temperature
 *   TODO 3: face 1 - seconds arc + hh:mm:ss
 *   TODO 4: face 2 - the weather complication
 *   TODO 5: blink the blue LED once per second (watch "tick")
 *
 * ------------------------- API quick reference -------------------------
 * Buttons/LEDs/sensor (common/hal.h):
 *   bool hal_button_pressed(int btn);    true once per press (edge)
 *   void hal_led(int led, bool on);      e.g. HACKPAD_LED_BLUE
 *   int  hal_bmp280_read(&temp_c, &press_hpa);
 *       Reads the Day 4 driver's sysfs attributes. Returns 0 on
 *       success. Slow-ish (one forced measurement) - call it from the
 *       5 s sensor timer, never from clock_tick().
 *
 * LVGL:
 *   lv_label_set_text(lbl, "12:34");             set label text
 *   lv_label_set_text_fmt(lbl, "%d.%d °C", ...); printf-style
 *   lv_arc_set_value(f1_arc, seconds);           move the arc (0..59)
 *
 * Time (C library):
 *   tm.tm_hour, tm.tm_min, tm.tm_sec             already decoded
 *   strftime(buf, sizeof(buf), "%a %d %b %Y", &tm);   date string
 *   snprintf(buf, sizeof(buf), "%02d:%02d", h, m);    zero-padded
 *
 * Display a double with one decimal without float printf:
 *   (int)temp_c, (int)(temp_c * 10) % 10          -> "%d.%d"
 *
 * -----------------------------------------------------------------------
 */
#include <stdio.h>
#include <time.h>
#include "hal.h"

#define NUM_FACES 3

static lv_obj_t *faces[NUM_FACES];

/* face 0 */
static lv_obj_t *f0_time, *f0_date, *f0_temp;
/* face 1 */
static lv_obj_t *f1_arc, *f1_time;
/* face 2 */
static lv_obj_t *f2_time, *f2_temp, *f2_press;

static int cur_face;
static double temp_c = -1000, press_hpa = -1000;	/* -1000 = no data */

/* Show face idx, hide the others */
static void show_face(int idx)
{
	for (int i = 0; i < NUM_FACES; i++) {
		if (i == idx)
			lv_obj_remove_flag(faces[i], LV_OBJ_FLAG_HIDDEN);
		else
			lv_obj_add_flag(faces[i], LV_OBJ_FLAG_HIDDEN);
	}
	cur_face = idx;
}

/* Every 5 s: refresh the cached sensor values */
static void sensor_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	hal_bmp280_read(&temp_c, &press_hpa);
}

/* Every 100 ms: buttons, LED tick, and the visible face */
static void clock_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	time_t now = time(NULL);
	struct tm tm;

	localtime_r(&now, &tm);

	/* TODO 1: face switching.
	 * SW2 -> show_face((cur_face + 1) % NUM_FACES)
	 * SW1 -> show_face((cur_face + NUM_FACES - 1) % NUM_FACES)
	 */

	/* TODO 5: watch tick.
	 * Blink the blue LED once per second, e.g. on for even seconds:
	 *   hal_led(HACKPAD_LED_BLUE, tm.tm_sec % 2 == 0);
	 */

	/* TODO 2/3/4: update the current face (switch on cur_face).
	 *
	 * face 0: f0_time "HH:MM" (montserrat 48 - it fits), f0_date via
	 *         strftime, f0_temp from temp_c (check > -999 first,
	 *         show "-- °C" otherwise).
	 * face 1: lv_arc_set_value(f1_arc, tm.tm_sec) and f1_time
	 *         "HH:MM:SS".
	 * face 2: f2_time "HH:MM", f2_temp and f2_press from the cached
	 *         sensor values.
	 *
	 * Only the visible face needs updating - the others are hidden.
	 */
}

static lv_obj_t *make_face(void)
{
	lv_obj_t *f = lv_obj_create(lv_screen_active());

	lv_obj_set_size(f, 240, 240);
	lv_obj_set_pos(f, 0, 0);
	lv_obj_set_style_bg_color(f, lv_color_black(), 0);
	lv_obj_set_style_border_width(f, 0, 0);
	lv_obj_set_style_radius(f, 0, 0);
	lv_obj_set_style_pad_all(f, 0, 0);
	lv_obj_remove_flag(f, LV_OBJ_FLAG_SCROLLABLE);
	return f;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
			    uint32_t color, lv_align_t align, int y)
{
	lv_obj_t *l = lv_label_create(parent);

	lv_obj_set_style_text_font(l, font, 0);
	lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
	lv_obj_align(l, align, 0, y);
	lv_label_set_text(l, "--");
	return l;
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* --- face 0: digital --- */
	faces[0] = make_face();
	f0_time = make_label(faces[0], &lv_font_montserrat_48, 0xFFFFFF,
			     LV_ALIGN_CENTER, -20);
	f0_date = make_label(faces[0], &lv_font_montserrat_14, 0x00C0FF,
			     LV_ALIGN_CENTER, 24);
	f0_temp = make_label(faces[0], &lv_font_montserrat_24, 0xFFA000,
			     LV_ALIGN_CENTER, 60);

	/* --- face 1: seconds arc --- */
	faces[1] = make_face();
	f1_arc = lv_arc_create(faces[1]);
	lv_obj_set_size(f1_arc, 220, 220);
	lv_obj_center(f1_arc);
	lv_arc_set_rotation(f1_arc, 270);
	lv_arc_set_bg_angles(f1_arc, 0, 360);
	lv_arc_set_range(f1_arc, 0, 59);
	lv_obj_remove_style(f1_arc, NULL, LV_PART_KNOB);
	lv_obj_remove_flag(f1_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(f1_arc, lv_color_hex(0x202020),
				   LV_PART_MAIN);
	lv_obj_set_style_arc_color(f1_arc, lv_color_hex(0x00FF80),
				   LV_PART_INDICATOR);
	f1_time = make_label(faces[1], &lv_font_montserrat_32, 0xFFFFFF,
			     LV_ALIGN_CENTER, 0);

	/* --- face 2: weather complication --- */
	faces[2] = make_face();
	f2_time = make_label(faces[2], &lv_font_montserrat_32, 0x808080,
			     LV_ALIGN_TOP_MID, 16);
	f2_temp = make_label(faces[2], &lv_font_montserrat_48, 0xFFA000,
			     LV_ALIGN_CENTER, 0);
	f2_press = make_label(faces[2], &lv_font_montserrat_24, 0x00C0FF,
			      LV_ALIGN_BOTTOM_MID, -24);

	show_face(0);
	sensor_tick(NULL);
	lv_timer_create(clock_tick, 100, NULL);
	lv_timer_create(sensor_tick, 5000, NULL);

	hal_run();
	return 0;
}
