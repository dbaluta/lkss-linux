// SPDX-License-Identifier: GPL-2.0
/*
 * chart - LVGL warm-up sample: a live scrolling plot
 *
 * A line chart in SHIFT mode: every new point pushes the old ones one
 * step to the left, so the chart scrolls like an oscilloscope. Here a
 * timer feeds it a synthetic sine wave - in the weather station the
 * same three lines plot the real BMP280 readings.
 *
 * Needs only st7789fb.ko (no buttons required).
 *
 * API used:
 *   lv_chart_create(parent)
 *   lv_chart_set_type(c, LV_CHART_TYPE_LINE)
 *   lv_chart_set_point_count(c, N)              visible history length
 *   lv_chart_set_update_mode(c, LV_CHART_UPDATE_MODE_SHIFT)
 *   lv_chart_add_series(c, color, LV_CHART_AXIS_PRIMARY_Y)
 *   lv_chart_set_next_value(c, series, v)       append one point
 *   lv_chart_set_axis_range(c, LV_CHART_AXIS_PRIMARY_Y, min, max)
 *
 * Chart values are integers. To plot a fractional quantity, scale it
 * (the weather station stores tenths: (int)(temp * 10)).
 */
#include <math.h>
#include "hal.h"

#define POINTS  60		/* visible history */

static lv_obj_t *chart;
static lv_chart_series_t *ser;
static lv_obj_t *value_lbl;

static void feed_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	static int i;

	/* Synthetic signal: sine between 10 and 90 */
	int v = 50 + (int)(40.0 * sin(i++ * 0.25));

	lv_chart_set_next_value(chart, ser, v);
	lv_label_set_text_fmt(value_lbl, "latest: %d", v);
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t *title = lv_label_create(scr);
	lv_label_set_text(title, "live chart");
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

	chart = lv_chart_create(scr);
	lv_obj_set_size(chart, 224, 160);
	lv_obj_align(chart, LV_ALIGN_CENTER, 0, 4);
	lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
	lv_chart_set_point_count(chart, POINTS);
	lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
	lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
	lv_obj_set_style_bg_color(chart, lv_color_hex(0x101010), 0);
	lv_obj_set_style_border_color(chart, lv_color_hex(0x303030), 0);
	lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR); /* no dots */

	ser = lv_chart_add_series(chart, lv_color_hex(0x00FF80),
				  LV_CHART_AXIS_PRIMARY_Y);

	value_lbl = lv_label_create(scr);
	lv_obj_set_style_text_color(value_lbl, lv_color_hex(0x808080), 0);
	lv_obj_align(value_lbl, LV_ALIGN_BOTTOM_MID, 0, -8);
	lv_label_set_text(value_lbl, "");

	lv_timer_create(feed_tick, 100, NULL);

	hal_run();
	return 0;
}
