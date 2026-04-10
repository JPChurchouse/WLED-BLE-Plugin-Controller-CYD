#include "display_setup.hpp"
#include <lvgl.h>

LGFX gfx;

// Two equal-sized buffers enable DMA double-buffering.
// 320 × 30 rows × 2 bytes = 19,200 bytes each → ~38 KB total.
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t         s_buf1[DISP_WIDTH * LVGL_BUF_LINES];
static lv_color_t         s_buf2[DISP_WIDTH * LVGL_BUF_LINES];

// ── LVGL flush callback ───────────────────────────────────────────────────
// Called by LVGL when a rectangular region is ready to be pushed to the LCD.
// LovyanGFX's writePixels() is DMA-capable, so this returns almost immediately.
static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    const uint32_t w = area->x2 - area->x1 + 1;
    const uint32_t h = area->y2 - area->y1 + 1;
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    // rgb565_t cast — LovyanGFX writes native 16-bit pixels
    gfx.writePixels(reinterpret_cast<lgfx::rgb565_t*>(&color_p->full), w * h);
    gfx.endWrite();
    lv_disp_flush_ready(drv);
}

// ── LVGL touch read callback ──────────────────────────────────────────────
// Called by LVGL's input driver at LV_INDEV_DEF_READ_PERIOD intervals.
// LovyanGFX's getTouch() returns calibrated, rotation-corrected pixel coords.
static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t x = 0, y = 0;
    if (gfx.getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = static_cast<lv_coord_t>(x);
        data->point.y = static_cast<lv_coord_t>(y);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void display_init() {
    gfx.init();
    gfx.setRotation(1);          // landscape: logical 320 × 240
    gfx.setBrightness(200);      // 0–255; 200 ≈ 78%
    gfx.fillScreen(TFT_BLACK);

    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, DISP_WIDTH * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISP_WIDTH;
    disp_drv.ver_res  = DISP_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);
}