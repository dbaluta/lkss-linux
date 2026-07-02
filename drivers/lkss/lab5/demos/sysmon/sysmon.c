// SPDX-License-Identifier: GPL-2.0
/*
 * sysmon - retro system monitor (PROJECT SKELETON - fill in the TODOs)
 *
 * Goal: a green-on-black CRT-style dashboard showing live data from
 * procfs - the same interface top(1), free(1) and uptime(1) use:
 * kernel version, uptime, load average, CPU % and memory bars. The
 * LED shows the CPU load at a glance.
 *
 * The retro layout (UNSCII pixel fonts, bars, blinking cursor) and
 * the uptime/loadavg parsing are already written as examples of the
 * fopen/fscanf pattern. You implement:
 *
 *   TODO 1: CPU usage from /proc/stat
 *   TODO 2: memory usage from /proc/meminfo
 *   TODO 3: CPU load LED (green < 30 %, blue < 70 %, red above)
 *
 * ------------------------- API quick reference -------------------------
 * LEDs (common/hal.h):
 *   void hal_leds(uint32_t mask);        bit0=red, bit1=green, bit2=blue
 *
 * LVGL:
 *   lv_label_set_text_fmt(lbl, "cpu %3d%%", v);   note %% for a literal %
 *   lv_bar_set_value(bar, v, LV_ANIM_OFF);        bars go 0..100
 *
 * procfs formats:
 *   /proc/stat, first line ("cpu" = sum of all cores, in clock ticks):
 *     cpu  user nice system idle iowait irq softirq ...
 *   CPU usage must be computed as a *delta* between two reads:
 *     total = user+nice+system+idle+iowait+irq+softirq
 *     usage% = 100 - (idle_delta * 100 / total_delta)
 *   (keep the previous total/idle in static variables; include iowait
 *   in idle. First call has no delta yet - return 0.)
 *
 *   /proc/meminfo (values in kB):
 *     MemTotal:       2013840 kB
 *     MemAvailable:   1704392 kB
 *   used% = (MemTotal - MemAvailable) * 100 / MemTotal
 *
 * Parsing hints:
 *   fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu", ...)
 *   fscanf(f, "%63s %ld kB\n", key, &val) in a loop + strcmp(key, ...)
 *
 * -----------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include "hal.h"

#define GREEN 0x33FF66
#define DARK  0x0A2A12

static lv_obj_t *uptime_lbl, *load_lbl, *cpu_lbl, *mem_lbl, *cursor_lbl;
static lv_obj_t *cpu_bar, *mem_bar;

/* Return the CPU usage in percent, or -1 on error */
static int read_cpu_percent(void)
{
	/* TODO 1: parse the first line of /proc/stat and compute the
	 * usage from the delta with the previous call (see the header
	 * comment for the formula). You need two static variables to
	 * remember the previous total and idle counters.
	 */
	return -1;
}

/* Return the memory usage in percent (and MB through the pointers),
 * or -1 on error */
static int read_mem_percent(long *used_mb, long *total_mb)
{
	/* TODO 2: parse MemTotal and MemAvailable from /proc/meminfo
	 * (loop with fscanf + strcmp, see the header comment), then:
	 *   *used_mb  = (total - avail) / 1024;   (kB -> MB)
	 *   *total_mb = total / 1024;
	 *   return (total - avail) * 100 / total;
	 */
	LV_UNUSED(used_mb);
	LV_UNUSED(total_mb);
	return -1;
}

/* Every second: refresh all the readings */
static void update_tick(lv_timer_t *t)
{
	LV_UNUSED(t);
	char buf[96];

	/* Uptime - given as an example of the procfs pattern */
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

	/* Load average - also given */
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

		/* TODO 3: CPU load LED.
		 * hal_leds(): green below 30 %, blue below 70 %, red
		 * above.
		 */
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

/* Blinking terminal cursor for the retro feel */
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

	lv_label_set_text(cpu_lbl, "cpu  TODO");
	lv_label_set_text(mem_lbl, "mem  TODO");

	update_tick(NULL);
	lv_timer_create(update_tick, 1000, NULL);
	lv_timer_create(cursor_tick, 500, NULL);

	hal_run();
	return 0;
}
