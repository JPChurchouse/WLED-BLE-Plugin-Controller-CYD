/**
 * lv_conf.h — LVGL 8.3.x configuration for ESP32 CYD (320×240, no PSRAM)
 */
#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>

#if 1   /* MUST be 1 — LVGL checks this guard */

/*====================
   COLOUR / DISPLAY
 *====================*/
#define LV_COLOR_DEPTH          16      /* ILI9341 is 16-bit RGB565          */
#define LV_COLOR_16_SWAP         0      /* LovyanGFX handles byte order itself */
#define LV_COLOR_SCREEN_TRANSP   0

/*====================
   MEMORY
 *====================*/
/* LVGL internal heap — drawn from ESP32 DRAM (no PSRAM on CYD)             */
#define LV_MEM_CUSTOM   0
#define LV_MEM_SIZE    (32U * 1024U)    /* 32 KB — plenty for this UI        */
#define LV_MEM_ADR      0               /* 0 = let LVGL pick                 */

/*====================
   HAL
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD   16   /* ms ≈ 60 Hz                         */
#define LV_INDEV_DEF_READ_PERIOD  16   /* ms — matches display refresh        */
#define LV_DPI_DEF               100

/*====================
   DRAWING
 *====================*/
#define LV_DRAW_COMPLEX           1
#define LV_SHADOW_CACHE_SIZE      0
#define LV_CIRCLE_CACHE_SIZE      4
#define LV_IMG_CACHE_DEF_SIZE     0
#define LV_GRADIENT_MAX_STOPS     2
#define LV_GRAD_CACHE_DEF_SIZE    0
#define LV_DITHER_GRADIENT        0
#define LV_DISP_ROT_MAX_BUF      (10 * 1024)

/*====================
   LOGGING / DEBUG
 *====================*/
#define LV_USE_LOG                0     /* disable for production             */
#define LV_USE_PERF_MONITOR       0
#define LV_USE_MEM_MONITOR        0
#define LV_USE_ASSERT_NULL        0
#define LV_USE_ASSERT_MALLOC      0
#define LV_USE_ASSERT_STYLE       0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ         0

/*====================
   FONTS
 *====================*/
/* Montserrat subset — only enable what you use; each adds ~10–40 KB flash   */
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_14   1   /* list items, labels                    */
#define LV_FONT_MONTSERRAT_16   0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   1   /* power button label                    */
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0
#define LV_FONT_MONTSERRAT_34   0
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_MONTSERRAT_38   0
#define LV_FONT_MONTSERRAT_40   0
#define LV_FONT_MONTSERRAT_42   0
#define LV_FONT_MONTSERRAT_44   0
#define LV_FONT_MONTSERRAT_46   0
#define LV_FONT_MONTSERRAT_48   0

#define LV_FONT_DEFAULT         &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE   0
#define LV_USE_FONT_SUBPX       0
#define LV_USE_FONT_PLACEHOLDER 1

/*====================
   TEXT
 *====================*/
#define LV_TXT_ENC              LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS      " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD        '#'
#define LV_USE_BIDI             0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC          0
#define LV_USE_BAR          0
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    0
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMG          0
#define LV_USE_LABEL        1
#define LV_USE_LINE         0
#define LV_USE_ROLLER       0
#define LV_USE_SLIDER       0
#define LV_USE_SWITCH       0
#define LV_USE_TEXTAREA     0
#define LV_USE_TABLE        0

/* Extra widgets */
#define LV_USE_CALENDAR     0
#define LV_USE_CHART        0
#define LV_USE_COLORWHEEL   0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LED          0
#define LV_USE_LIST         0   /* using a plain flex container instead      */
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      1   /* connecting-screen animation               */
#define LV_USE_TABVIEW      0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 1
    #define LV_THEME_DEFAULT_DARK            1   /* dark theme                */
    #define LV_THEME_DEFAULT_GROW            0
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_SIMPLE  0
#define LV_USE_THEME_MONO    0

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX         1   /* used for scrollable preset container      */
#define LV_USE_GRID         0

/*====================
   ANIMATIONS
 *====================*/
#define LV_USE_ANIMATION    1   /* screen fade transitions                   */

/*====================
   FILE SYSTEM / GPU / MISC
 *====================*/
#define LV_USE_FS_STDIO     0
#define LV_USE_FS_POSIX     0
#define LV_USE_FS_WIN32     0
#define LV_USE_FS_FATFS     0
#define LV_USE_PNG          0
#define LV_USE_BMP          0
#define LV_USE_SJPG         0
#define LV_USE_GIF          0
#define LV_USE_QRCODE       0
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0
#define LV_USE_SNAPSHOT         0
#define LV_USE_MONKEY           0
#define LV_USE_GRIDNAV          0
#define LV_USE_FRAGMENT         0
#define LV_USE_IMGFONT          0

#endif  /* guard */
#endif  /* LV_CONF_H */