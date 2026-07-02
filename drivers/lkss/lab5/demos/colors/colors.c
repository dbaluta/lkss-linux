// SPDX-License-Identifier: GPL-2.0
/*
 * colors - LVGL warm-up sample #1
 *
 * Paints the whole screen with a new color every second and prints the
 * color name in the middle. The smallest possible "is my display
 * pipeline alive?" LVGL application: one object (the screen), one
 * label, one timer.
 *
 * Needs only st7789fb.ko (no buttons required).
 */
#include "hal.h"

struct named_color {
	const char *name;
	uint32_t rgb;
};

static const struct named_color palette[] = {
	{ "RED",     0xFF0000 },
	{ "GREEN",   0x00FF00 },
	{ "BLUE",    0x0000FF },
	{ "YELLOW",  0xFFFF00 },
	{ "CYAN",    0x00FFFF },
	{ "MAGENTA", 0xFF00FF },
	{ "WHITE",   0xFFFFFF },
	{ "BLACK",   0x000000 },
};

static lv_obj_t *name_lbl;

static void next_color(lv_timer_t *t)
{
	LV_UNUSED(t);
	static unsigned int i;
	const struct named_color *c =
		&palette[i % (sizeof(palette) / sizeof(palette[0]))];

	/* Fill the screen by changing its background style... */
	lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(c->rgb), 0);

	/* ...and keep the label readable on any background */
	lv_label_set_text(name_lbl, c->name);
	lv_obj_set_style_text_color(name_lbl,
			(c->rgb == 0x000000 || c->rgb == 0x0000FF) ?
			lv_color_white() : lv_color_black(), 0);

	i++;
}

int main(void)
{
	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	name_lbl = lv_label_create(scr);
	lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_32, 0);
	lv_obj_center(name_lbl);

	next_color(NULL);			/* first color right away */
	lv_timer_create(next_color, 1000, NULL);

	hal_run();
	return 0;
}
