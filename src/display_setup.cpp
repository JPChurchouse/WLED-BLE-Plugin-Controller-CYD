#include "display_setup.hpp"
#include <lvgl.h>

LGFX gfx;

// Buffer sized for landscape (320 wide). Single buffer — no PSRAM.
// 320 × 10 × 2 bytes = 6,400 bytes
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t         s_buf[320 * LVGL_BUF_LINES];

// ── Flush callback ────────────────────────────────────────────────────────
// pushImage is fully atomic: CS assertion → CASET/PASET → RAMWR → pixel DMA
// → CS deassert in a single SPI transaction. No split-transaction race.
//
// LV_COLOR_16_SWAP=0: LVGL hands us native little-endian RGB565.
// LovyanGFX's pushImage(rgb565_t*) pipeline byte-swaps to big-endian
// internally before writing to the ILI9341 SPI bus.
static void disp_flush(lv_disp_drv_t* drv,
                       const lv_area_t* area,
                       lv_color_t* color_p)
{
    gfx.pushImage(area->x1,
                  area->y1,
                  area->x2 - area->x1 + 1,
                  area->y2 - area->y1 + 1,
                  reinterpret_cast<lgfx::rgb565_t*>(color_p));
    lv_disp_flush_ready(drv);
}

// ── Touch read callback ───────────────────────────────────────────────────
static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    uint16_t x = 0, y = 0;
    if (gfx.getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = static_cast<lv_coord_t>(x);
        data->point.y = static_cast<lv_coord_t>(y);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void display_init()
{
    gfx.init();

    // ── Clear GRAM in the panel's native portrait orientation ─────────────
    // The ILI9341 GRAM is 240×320. Any content left by a previous sketch
    // lives here regardless of what rotation that sketch used. Clearing in
    // native orientation guarantees every cell is zeroed before we rotate.
    gfx.setRotation(0);
    gfx.fillScreen(TFT_BLACK);

    // ── Switch to landscape and clear again ───────────────────────────────
    // After MADCTL is updated the address window remaps; fill again so that
    // the landscape GRAM view is also clean.
    gfx.setRotation(1);     // landscape: USB connector at the right
    gfx.fillScreen(TFT_BLACK);
    gfx.setBrightness(220);

    // Diagnostic — confirms rotation took effect before LVGL starts
    Serial.printf("[DISP] post-rotation: %d x %d\n", gfx.width(), gfx.height());

    // ── LVGL init ─────────────────────────────────────────────────────────
    lv_init();

    // Use actual hardware dimensions after rotation — avoids hard-coding
    // assumptions about which way setRotation mapped the GRAM.
    const uint16_t W = static_cast<uint16_t>(gfx.width());
    const uint16_t H = static_cast<uint16_t>(gfx.height());

    lv_disp_draw_buf_init(&s_draw_buf, s_buf, nullptr, W * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = W;
    disp_drv.ver_res  = H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);
}