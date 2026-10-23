// LVGL's settings this build changes from its lv_conf_template.h, or rests a figure on

#define LV_COLOR_FORMAT_DEFAULT     LV_COLOR_FORMAT_RGB565_SWAPPED   // RGB565 high byte first, as the driver sends it
#define LV_USE_OS                   LV_OS_NONE      // LVGL runs in the main loop only, so it needs no lock
#define LV_DEF_REFR_PERIOD          33              // LVGL's default, longer than a whole screen takes on the bus, docs/parts/display
#define LV_MEM_SIZE                 (48U * 1024U)   // LVGL's heap, the size its requirements recommend for many widgets; in RAM1

// LVGL's code stays in the flash, which a linker step sees to; its drawing loops run from RAM1
#define LV_ATTRIBUTE_FAST_MEM       FASTRUN
#define LV_ATTRIBUTE_LARGE_CONST    PROGMEM         // fonts and tables stay in the flash
