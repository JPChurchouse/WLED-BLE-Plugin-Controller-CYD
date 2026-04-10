#pragma once

// LovyanGFX must be included before lvgl so the display driver is ready
// when LVGL calls the flush callback.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.hpp"

// ── CYD display/touch driver class ───────────────────────────────────────
// Two separate SPI buses are used:
//   SPI2 (HSPI) — ILI9341 display: CLK=14, MOSI=13, MISO=12
//   SPI3 (VSPI) — XPT2046 touch:   CLK=25, MOSI=32, MISO=39
// Both are driven by LovyanGFX simultaneously without bus contention.
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341  _panel;
    lgfx::Bus_SPI        _display_bus;
    lgfx::Light_PWM      _backlight;
    lgfx::Touch_XPT2046  _touch;

public:
    LGFX() {
        // ── Display SPI bus ──────────────────────────────────────────────
        {
            auto cfg = _display_bus.config();
            // SPI2_HOST = 1 (HSPI). If this symbol isn't found on your
            // toolchain, replace with the integer literal 1.
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = TFT_CLK;
            cfg.pin_mosi    = TFT_MOSI;
            cfg.pin_miso    = TFT_MISO;
            cfg.pin_dc      = TFT_DC;
            _display_bus.config(cfg);
            _panel.setBus(&_display_bus);
        }
        // ── Panel ────────────────────────────────────────────────────────
        {
            auto cfg = _panel.config();
            cfg.pin_cs           = TFT_CS;
            cfg.pin_rst          = -1;       // tied to EN/3V3
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;      // physical portrait short edge
            cfg.panel_height     = 320;      // physical portrait long edge
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = false;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel.config(cfg);
        }
        // ── Backlight ────────────────────────────────────────────────────
        {
            auto cfg = _backlight.config();
            cfg.pin_bl      = TFT_BL;
            cfg.invert      = false;         // HIGH = on (via NPN transistor)
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _backlight.config(cfg);
            _panel.setLight(&_backlight);
        }
        // ── Touch ────────────────────────────────────────────────────────
        {
            auto cfg = _touch.config();
            cfg.x_min           = TOUCH_X_MIN;
            cfg.x_max           = TOUCH_X_MAX;
            cfg.y_min           = TOUCH_Y_MIN;
            cfg.y_max           = TOUCH_Y_MAX;
            cfg.pin_int         = TCH_IRQ;
            cfg.bus_shared      = false;     // dedicated SPI bus
            cfg.offset_rotation = 0;
            // SPI3_HOST = 2 (VSPI). Replace with 2 if symbol undefined.
            cfg.spi_host        = SPI3_HOST;
            cfg.freq            = 2500000;
            cfg.pin_sclk        = TCH_CLK;
            cfg.pin_mosi        = TCH_MOSI;
            cfg.pin_miso        = TCH_MISO;
            cfg.pin_cs          = TCH_CS;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};

// Global display object — defined in display_setup.cpp, used in flush CB
extern LGFX gfx;

// Initialises LovyanGFX + LVGL display/input drivers.
// Call once before any lv_* calls.
void display_init();