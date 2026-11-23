// LVGL's settings this build changes from its lv_conf_template.h, or rests a figure on. LVGL finds
// the file by its name on the include path. Every setting not named here keeps LVGL's default.
#ifndef LV_CONF_H
#define LV_CONF_H

// RGB565 high byte first, as the display driver sends it, so a picture in that format is copied as
// it is
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565_SWAPPED
// LVGL runs in the main loop only, so it needs no lock
#define LV_USE_OS LV_OS_NONE
// LVGL's default, longer than a whole screen takes on the bus; display model: t_refr
#define LV_DEF_REFR_PERIOD 33
// LVGL's heap in RAM1, the size LVGL's requirements recommend for many widgets
#define LV_MEM_SIZE (48U * 1024U)

// LVGL draws into the display's RGB565 high byte first and into ARGB8888 for a layer with
// transparency. Its pictures are RGB565 high byte first, or RGB565 with an opacity byte, and its
// fonts are opacities. Every other format's drawing loop would take RAM1 for nothing. RGB565 stays,
// since LVGL 9.6 builds its opacity format only beside it.
#define LV_DRAW_SW_SUPPORT_RGB888 0
#define LV_DRAW_SW_SUPPORT_XRGB8888 0
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8 0
#define LV_DRAW_SW_SUPPORT_AL88 0
#define LV_DRAW_SW_SUPPORT_I1 0

#if defined(ARDUINO_TEENSY41)
// FASTRUN and PROGMEM, which LVGL's C files otherwise never see
#include <avr/pgmspace.h>
// LVGL's code stays in the flash, which firmware/linker/teensy41.ld sees to, and its drawing loops
// run from RAM1
#define LV_ATTRIBUTE_FAST_MEM FASTRUN
// fonts and tables stay in the flash
#define LV_ATTRIBUTE_LARGE_CONST PROGMEM
#endif

#endif
