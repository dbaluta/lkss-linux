/**
 * lv_conf.h - LVGL configuration for the LKSS hackathon (i.MX93 FRDM)
 *
 * Only the options that differ from the LVGL defaults are set here;
 * everything else falls back to the defaults in lv_conf_internal.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* The ST7789 framebuffer is RGB565 */
#define LV_COLOR_DEPTH 16

/* Linux /dev/fb0 display backend (talks to st7789fb.ko) */
#define LV_USE_LINUX_FBDEV 1

/* Refresh at the panel's deferred-io rate */
#define LV_DEF_REFR_PERIOD 33

/* Fonts used by the demos */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

/* Retro pixel fonts (sysmon demo) */
#define LV_FONT_UNSCII_8  1
#define LV_FONT_UNSCII_16 1

#endif /* LV_CONF_H */
