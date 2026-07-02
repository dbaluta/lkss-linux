// SPDX-License-Identifier: GPL-2.0
/*
 * watch - a wristwatch on the ST7789 for the LKSS hackathon
 *
 * Three watch faces, all fed by the system clock and the BMP280
 * (plausible random values stand in when the sensor is not available,
 * so the watch runs on any board - a red warning at the bottom of the
 * screen then flags the values as fake):
 *   face 0: big digital HH:MM:SS clock + date + temperature/pressure
 *   face 1: seconds arc around the time
 *   face 2: temperature/pressure "complication" face
 *
 * Controls: SW1 = previous face, SW2 = next face
 * The blue LED blinks once per second (like a watch tick).
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "hal.h"

#define NUM_FACES 3

static lv_obj_t *faces[NUM_FACES];

/* face 0 */
static lv_obj_t *f0_time, *f0_date, *f0_temp, *f0_press;
/* face 1 */
static lv_obj_t *f1_arc, *f1_time;
/* face 2 */
static lv_obj_t *f2_time, *f2_temp, *f2_press;
/* shown on top of every face while the sensor values are fake */
static lv_obj_t *warn_lbl;

static int cur_face;
static double temp_c, press_hpa;	/* cached sensor (or random) values */

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

/* Without a BMP280 (bmp280.ko not loaded, sensor not wired) plausible
 * random values stand in, so the watch runs on any board - but the
 * warning label makes it clear the readings are fake.
 */
static void sensor_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	if (hal_bmp280_read(&temp_c, &press_hpa) == 0) {
		lv_obj_add_flag(warn_lbl, LV_OBJ_FLAG_HIDDEN);
		return;
	}

	temp_c = 18.0 + rand() % 150 / 10.0;	/* 18.0 .. 32.9 */
	press_hpa = 990 + rand() % 41;		/* 990 .. 1030  */
	lv_obj_remove_flag(warn_lbl, LV_OBJ_FLAG_HIDDEN);
}

static void clock_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	static int last_sec = -1;
	time_t now = time(NULL);
	struct tm tm;

	localtime_r(&now, &tm);

	if (hal_button_pressed(HACKPAD_BTN_SW2))
		show_face((cur_face + 1) % NUM_FACES);
	if (hal_button_pressed(HACKPAD_BTN_SW1))
		show_face((cur_face + NUM_FACES - 1) % NUM_FACES);

	/* Tick LED: on for even seconds, off for odd */
	if (tm.tm_sec != last_sec) {
		last_sec = tm.tm_sec;
		hal_led(HACKPAD_LED_BLUE, tm.tm_sec % 2 == 0);
	}

	char buf[64];

	switch (cur_face) {
	case 0:
		snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
			 tm.tm_hour, tm.tm_min, tm.tm_sec);
		lv_label_set_text(f0_time, buf);
		strftime(buf, sizeof(buf), "%a %d %b %Y", &tm);
		lv_label_set_text(f0_date, buf);
		lv_label_set_text_fmt(f0_temp, "%d.%d °C", (int)temp_c,
				      (int)(temp_c * 10) % 10);
		lv_label_set_text_fmt(f0_press, "%d hPa", (int)press_hpa);
		break;
	case 1:
		lv_arc_set_value(f1_arc, tm.tm_sec);
		snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
			 tm.tm_hour, tm.tm_min, tm.tm_sec);
		lv_label_set_text(f1_time, buf);
		break;
	case 2:
		snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
		lv_label_set_text(f2_time, buf);
		lv_label_set_text_fmt(f2_temp, "%d.%d °C", (int)temp_c,
				      (int)(temp_c * 10) % 10);
		lv_label_set_text_fmt(f2_press, "%d hPa", (int)press_hpa);
		break;
	}
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
	lv_label_set_text(l, "");
	return l;
}

int main(void)
{
	srand(time(NULL));
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* --- face 0: digital --- */
	faces[0] = make_face();
	f0_time = make_label(faces[0], &lv_font_montserrat_48, 0xFFFFFF,
			     LV_ALIGN_CENTER, -32);
	f0_date = make_label(faces[0], &lv_font_montserrat_14, 0x00C0FF,
			     LV_ALIGN_CENTER, 8);
	f0_temp = make_label(faces[0], &lv_font_montserrat_24, 0xFFA000,
			     LV_ALIGN_CENTER, 44);
	f0_press = make_label(faces[0], &lv_font_montserrat_18, 0x00FF80,
			      LV_ALIGN_CENTER, 76);

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

	/* sensor warning, created last so it sits on top of every face */
	warn_lbl = lv_label_create(scr);
	lv_obj_set_style_text_font(warn_lbl, &lv_font_montserrat_12, 0);
	lv_obj_set_style_text_color(warn_lbl, lv_color_hex(0xFF4040), 0);
	lv_obj_align(warn_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
	lv_label_set_text(warn_lbl,
			  LV_SYMBOL_WARNING " no sensor - fake values");
	lv_obj_add_flag(warn_lbl, LV_OBJ_FLAG_HIDDEN);

	show_face(0);
	sensor_tick(NULL);
	lv_timer_create(clock_tick, 100, NULL);
	lv_timer_create(sensor_tick, 5000, NULL);

	hal_run();
	return 0;
}
