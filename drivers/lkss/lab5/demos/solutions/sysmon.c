// SPDX-License-Identifier: GPL-2.0
/*
 * sysmon - retro terminal system monitor for the LKSS hackathon
 *
 * Green-on-black CRT look showing live data straight from procfs:
 * kernel version, uptime, load average, CPU usage and memory usage.
 * The LED shows the CPU load: green < 30 %, blue < 70 %, red above.
 *
 * Everything here is read from /proc - the same interface used by
 * top(1), free(1) and uptime(1).
 */
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "hal.h"

#define GREEN 0x33FF66
#define DARK  0x0A2A12

static lv_obj_t *uptime_lbl, *load_lbl, *cpu_lbl, *mem_lbl, *cursor_lbl;
static lv_obj_t *cpu_bar, *mem_bar;

/* Previous /proc/stat counters for CPU usage delta */
static unsigned long long prev_total, prev_idle;

static int read_cpu_percent(void)
{
	FILE *f = fopen("/proc/stat", "r");
	unsigned long long user, nice, sys, idle, iowait, irq, softirq;

	if (!f)
		return -1;
	if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu",
		   &user, &nice, &sys, &idle, &iowait, &irq, &softirq) != 7) {
		fclose(f);
		return -1;
	}
	fclose(f);

	unsigned long long total = user + nice + sys + idle + iowait +
				   irq + softirq;
	unsigned long long dtotal = total - prev_total;
	unsigned long long didle = (idle + iowait) - prev_idle;

	prev_total = total;
	prev_idle = idle + iowait;

	if (dtotal == 0)
		return 0;
	return (int)(100 - didle * 100 / dtotal);
}

static int read_mem_percent(long *used_mb, long *total_mb)
{
	FILE *f = fopen("/proc/meminfo", "r");
	long total = 0, avail = 0;
	char key[64];
	long val;

	if (!f)
		return -1;
	while (fscanf(f, "%63s %ld kB\n", key, &val) == 2) {
		if (!strcmp(key, "MemTotal:"))
			total = val;
		else if (!strcmp(key, "MemAvailable:"))
			avail = val;
		if (total && avail)
			break;
	}
	fclose(f);

	if (!total)
		return -1;
	*used_mb = (total - avail) / 1024;
	*total_mb = total / 1024;
	return (int)((total - avail) * 100 / total);
}

static void update_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	char buf[96];

	/* Uptime */
	FILE *f = fopen("/proc/uptime", "r");
	double up = 0;

	if (f) {
		if (fscanf(f, "%lf", &up) != 1)
			up = 0;
		fclose(f);
	}
	snprintf(buf, sizeof(buf), "up  %02d:%02d:%02d",
		 (int)up / 3600, ((int)up / 60) % 60, (int)up % 60);
	lv_label_set_text(uptime_lbl, buf);

	/* Load average */
	f = fopen("/proc/loadavg", "r");
	if (f) {
		double l1 = 0, l5 = 0, l15 = 0;

		if (fscanf(f, "%lf %lf %lf", &l1, &l5, &l15) == 3) {
			snprintf(buf, sizeof(buf),
				 "load %.2f %.2f %.2f", l1, l5, l15);
			lv_label_set_text(load_lbl, buf);
		}
		fclose(f);
	}

	/* CPU */
	int cpu = read_cpu_percent();

	if (cpu >= 0) {
		lv_label_set_text_fmt(cpu_lbl, "cpu  %3d%%", cpu);
		lv_bar_set_value(cpu_bar, cpu, LV_ANIM_OFF);
		if (cpu < 30)
			hal_leds(1 << HACKPAD_LED_GREEN);
		else if (cpu < 70)
			hal_leds(1 << HACKPAD_LED_BLUE);
		else
			hal_leds(1 << HACKPAD_LED_RED);
	}

	/* Memory */
	long used_mb, total_mb;
	int mem = read_mem_percent(&used_mb, &total_mb);

	if (mem >= 0) {
		lv_label_set_text_fmt(mem_lbl, "mem  %3d%%  %ld/%ldMB",
				      mem, used_mb, total_mb);
		lv_bar_set_value(mem_bar, mem, LV_ANIM_OFF);
	}
}

static void cursor_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	static bool on;

	on = !on;
	lv_label_set_text(cursor_lbl, on ? "_" : " ");
}

static lv_obj_t *make_line(lv_obj_t *parent, int y, const lv_font_t *font)
{
	lv_obj_t *l = lv_label_create(parent);

	lv_obj_set_style_text_color(l, lv_color_hex(GREEN), 0);
	lv_obj_set_style_text_font(l, font, 0);
	lv_obj_set_pos(l, 8, y);
	lv_label_set_text(l, "");
	return l;
}

static lv_obj_t *make_bar(lv_obj_t *parent, int y)
{
	lv_obj_t *b = lv_bar_create(parent);

	lv_obj_set_size(b, 224, 10);
	lv_obj_set_pos(b, 8, y);
	lv_bar_set_range(b, 0, 100);
	lv_obj_set_style_bg_color(b, lv_color_hex(DARK), LV_PART_MAIN);
	lv_obj_set_style_bg_color(b, lv_color_hex(GREEN), LV_PART_INDICATOR);
	lv_obj_set_style_radius(b, 2, LV_PART_MAIN);
	lv_obj_set_style_radius(b, 2, LV_PART_INDICATOR);
	return b;
}

int main(void)
{
	struct utsname un;

	hal_init();

	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* Header: hostname + kernel release */
	uname(&un);
	lv_obj_t *hdr = make_line(scr, 8, &lv_font_unscii_16);
	lv_label_set_text_fmt(hdr, "%s@lkss", un.nodename);

	lv_obj_t *krn = make_line(scr, 30, &lv_font_unscii_8);
	lv_label_set_text_fmt(krn, "linux %s %s", un.release, un.machine);

	lv_obj_t *sep = make_line(scr, 46, &lv_font_unscii_8);
	lv_label_set_text(sep, "------------------------------");

	uptime_lbl = make_line(scr,  62, &lv_font_unscii_16);
	load_lbl   = make_line(scr,  86, &lv_font_unscii_16);
	cpu_lbl    = make_line(scr, 118, &lv_font_unscii_16);
	cpu_bar    = make_bar(scr, 140);
	mem_lbl    = make_line(scr, 160, &lv_font_unscii_16);
	mem_bar    = make_bar(scr, 182);
	cursor_lbl = make_line(scr, 204, &lv_font_unscii_16);

	update_tick(NULL);
	lv_timer_create(update_tick, 1000, NULL);
	lv_timer_create(cursor_tick, 500, NULL);

	hal_run();
	return 0;
}
