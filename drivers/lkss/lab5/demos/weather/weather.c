// SPDX-License-Identifier: GPL-2.0
/*
 * weather - BMP280 weather station (PROJECT SKELETON - fill in TODOs)
 *
 * Goal: a multi-screen weather station on top of your Day 4 driver:
 *   screen 0: live temperature + pressure + trend
 *   screen 1: temperature history chart (last 2 minutes)
 *   screen 2: pressure history chart (last 2 minutes)
 * Falling pressure predicts bad weather - show the tendency and light
 * the matching LED (green = rising, red = falling, blue = steady).
 *
 * Controls: SW1 = previous screen, SW2 = next screen
 *
 * The three screens, the labels and the two charts are already
 * created, and sample_tick() already reads the sensor every second,
 * keeps the pressure history and updates the live labels. You
 * implement:
 *
 *   TODO 1: navigate the screens with SW1/SW2
 *   TODO 2: feed the charts with each new sample
 *   TODO 3: keep the chart Y axis centered on the current value
 *   TODO 4: compute the pressure trend and drive the label + LED
 *
 * ------------------------- API quick reference -------------------------
 * Buttons/LEDs/sensor (common/hal.h):
 *   bool hal_button_pressed(int btn);    true once per press (edge)
 *   void hal_leds(uint32_t mask);        bit0=red, bit1=green, bit2=blue
 *   int  hal_bmp280_read(&temp_c, &press_hpa);   0 on success
 *
 * LVGL chart (screens 1/2; series already added):
 *   lv_chart_set_next_value(chart, series, v);
 *       Append one point; the chart scrolls left (SHIFT update mode).
 *       Values are integers - we store tenths: (int)(temp * 10).
 *   lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
 *       Set the visible Y range (also in tenths).
 *
 * Labels:
 *   lv_label_set_text(lbl, "trend: rising " LV_SYMBOL_UP);
 *   (LV_SYMBOL_UP / LV_SYMBOL_DOWN / LV_SYMBOL_MINUS are built-in icons)
 *
 * History ring buffer (already maintained in sample_tick):
 *   press_history[(press_count - 1) % HISTORY]   newest sample
 *   press_history[(press_count - 30) % HISTORY]  ~30 seconds ago
 *
 * -----------------------------------------------------------------------
 */
#include <stdio.h>
#include "hal.h"

#define NUM_SCREENS  3
#define HISTORY      120	/* samples */
#define SAMPLE_MS    1000

static lv_obj_t *screens[NUM_SCREENS];
static lv_obj_t *title_lbl;

/* screen 0 */
static lv_obj_t *temp_lbl, *press_lbl, *trend_lbl;
/* screens 1 + 2 */
static lv_obj_t *temp_chart, *press_chart;
static lv_chart_series_t *temp_ser, *press_ser;
static lv_obj_t *temp_minmax, *press_minmax;

static const char *titles[NUM_SCREENS] = {
	"NOW", "TEMPERATURE", "PRESSURE",
};

static int cur_screen;
static double press_history[HISTORY];
static int press_count;

/* Show screen idx, hide the others, update the title bar */
static void show_screen(int idx)
{
	for (int i = 0; i < NUM_SCREENS; i++) {
		if (i == idx)
			lv_obj_remove_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
		else
			lv_obj_add_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
	}
	lv_label_set_text_fmt(title_lbl, "< %s >", titles[idx]);
	cur_screen = idx;
}

/* Every 50 ms: menu navigation */
static void nav_tick(lv_timer_t *t)
{
	LV_UNUSED(t);

	/* TODO 1: navigation.
	 * SW2 -> show_screen((cur_screen + 1) % NUM_SCREENS)
	 * SW1 -> show_screen((cur_screen + NUM_SCREENS - 1) % NUM_SCREENS)
	 */
}

static void update_trend(void)
{
	/* TODO 4: pressure trend.
	 * Need at least 30 samples first; until then show
	 * "trend: measuring..." and light the blue LED.
	 * Then compare the newest sample with the one ~30 s ago (see the
	 * ring buffer indexing in the header comment):
	 *   delta > +0.05 hPa  -> "trend: rising"  + green LED
	 *   delta < -0.05 hPa  -> "trend: falling" + red LED
	 *   otherwise          -> "trend: steady"  + blue LED
	 * Update trend_lbl and hal_leds() accordingly.
	 */
}

/* Every second: read the sensor and refresh everything */
static void sample_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	double temp, press;

	if (hal_bmp280_read(&temp, &press) < 0) {
		lv_label_set_text(temp_lbl, "sensor?");
		return;
	}

	/* Keep the pressure history (ring buffer) */
	press_history[press_count % HISTORY] = press;
	press_count++;

	/* Live screen */
	lv_label_set_text_fmt(temp_lbl, "%d.%d °C",
			      (int)temp, (int)(temp * 10) % 10);
	lv_label_set_text_fmt(press_lbl, "%d.%d hPa",
			      (int)press, (int)(press * 10) % 10);
	update_trend();

	/* TODO 2: charts.
	 * Append the new sample to both charts in tenths:
	 *   lv_chart_set_next_value(temp_chart, temp_ser, (int)(temp * 10));
	 * and the same for press_chart/press_ser.
	 */

	/* TODO 3: axis range.
	 * A fixed range would clip the line sooner or later. Re-center
	 * the Y axis around the current value, +/- 3 units (30 tenths):
	 *   lv_chart_set_axis_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y,
	 *                           (int)(temp * 10) - 30,
	 *                           (int)(temp * 10) + 30);
	 * Also update temp_minmax / press_minmax with the latest value.
	 */
}

static lv_obj_t *make_screen(void)
{
	lv_obj_t *s = lv_obj_create(lv_screen_active());

	lv_obj_set_size(s, 240, 216);
	lv_obj_set_pos(s, 0, 24);
	lv_obj_set_style_bg_color(s, lv_color_black(), 0);
	lv_obj_set_style_border_width(s, 0, 0);
	lv_obj_set_style_radius(s, 0, 0);
	lv_obj_set_style_pad_all(s, 4, 0);
	lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
	return s;
}

static lv_obj_t *make_chart(lv_obj_t *parent, lv_chart_series_t **ser,
			    uint32_t color)
{
	lv_obj_t *c = lv_chart_create(parent);

	lv_obj_set_size(c, 224, 160);
	lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 4);
	lv_chart_set_type(c, LV_CHART_TYPE_LINE);
	lv_chart_set_point_count(c, HISTORY);
	lv_chart_set_update_mode(c, LV_CHART_UPDATE_MODE_SHIFT);
	lv_obj_set_style_bg_color(c, lv_color_hex(0x101010), 0);
	lv_obj_set_style_border_color(c, lv_color_hex(0x303030), 0);
	lv_obj_set_style_size(c, 0, 0, LV_PART_INDICATOR); /* no dots */
	*ser = lv_chart_add_series(c, lv_color_hex(color),
				   LV_CHART_AXIS_PRIMARY_Y);
	return c;
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

	/* --- screen 0: live values --- */
	screens[0] = make_screen();
	temp_lbl = lv_label_create(screens[0]);
	lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(temp_lbl, lv_color_hex(0xFFA000), 0);
	lv_obj_align(temp_lbl, LV_ALIGN_CENTER, 0, -50);
	lv_label_set_text(temp_lbl, "--");

	press_lbl = lv_label_create(screens[0]);
	lv_obj_set_style_text_font(press_lbl, &lv_font_montserrat_32, 0);
	lv_obj_set_style_text_color(press_lbl, lv_color_hex(0x00C0FF), 0);
	lv_obj_align(press_lbl, LV_ALIGN_CENTER, 0, 10);
	lv_label_set_text(press_lbl, "--");

	trend_lbl = lv_label_create(screens[0]);
	lv_obj_set_style_text_color(trend_lbl, lv_color_hex(0x808080), 0);
	lv_obj_align(trend_lbl, LV_ALIGN_CENTER, 0, 60);
	lv_label_set_text(trend_lbl, "");

	/* --- screen 1: temperature chart --- */
	screens[1] = make_screen();
	temp_chart = make_chart(screens[1], &temp_ser, 0xFFA000);
	temp_minmax = lv_label_create(screens[1]);
	lv_obj_set_style_text_color(temp_minmax, lv_color_hex(0x808080), 0);
	lv_obj_align(temp_minmax, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_label_set_text(temp_minmax, "");

	/* --- screen 2: pressure chart --- */
	screens[2] = make_screen();
	press_chart = make_chart(screens[2], &press_ser, 0x00C0FF);
	press_minmax = lv_label_create(screens[2]);
	lv_obj_set_style_text_color(press_minmax, lv_color_hex(0x808080), 0);
	lv_obj_align(press_minmax, LV_ALIGN_BOTTOM_MID, 0, -4);
	lv_label_set_text(press_minmax, "");

	show_screen(0);
	sample_tick(NULL);
	lv_timer_create(sample_tick, SAMPLE_MS, NULL);
	lv_timer_create(nav_tick, 50, NULL);

	hal_run();
	return 0;
}
