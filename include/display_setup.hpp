#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.hpp"

/*
 * ESP32-2432S028R (CYD) confirmed pin mapping
 * -------------------------------------------
 * Display ILI9341 — SPI2 (HSPI) — native ESP32 HSPI pins, no GPIO matrix routing
 *   MOSI=13  MISO=NC  SCLK=14  CS=15  DC=2  BL=21
 *   MISO is not connected to the display on this board revision.
 *   Setting it -1 prevents LovyanGFX from attempting read-back.
 *
 * Touch XPT2046 — SPI3 (VSPI) — routed through GPIO matrix
 *   MOSI=32  MISO=39  SCLK=25  CS=33  IRQ=36
 *
 * Both buses are independent — bus_shared=false on both sides.
 */

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341  _panel;
    lgfx::Bus_SPI        _display_bus;
    lgfx::Light_PWM      _backlight;
    lgfx::Touch_XPT2046  _touch;

public:
    LGFX() {
        // ── Display SPI bus (SPI2 / HSPI) ────────────────────────────────
        {
            auto cfg         = _display_bus.config();
            cfg.spi_host     = SPI2_HOST;   // HSPI — native pins, no matrix
            cfg.spi_mode     = 0;
            cfg.freq_write   = 27000000;    // ILI9341 datasheet max for write
            cfg.freq_read    = 8000000;
            cfg.spi_3wire    = false;       // separate DC pin = 4-wire
            cfg.use_lock     = true;
            cfg.dma_channel  = SPI_DMA_CH_AUTO;
            cfg.pin_sclk     = TFT_CLK;    // 14
            cfg.pin_mosi     = TFT_MOSI;   // 13
            cfg.pin_miso     = -1;         // not connected on this board
            cfg.pin_dc       = TFT_DC;     // 2
            _display_bus.config(cfg);
            _panel.setBus(&_display_bus);
        }

        // ── Panel ─────────────────────────────────────────────────────────
        {
            auto cfg             = _panel.config();
            cfg.pin_cs           = TFT_CS;   // 15
            cfg.pin_rst          = -1;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;       // ILI9341 GRAM is 240 wide
            cfg.memory_height    = 320;       // ILI9341 GRAM is 320 tall
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;         // physical origin — setRotation(1)
                                               // below gives landscape
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;     // MISO not connected
            cfg.invert           = false;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;     // touch is on a completely separate bus
            _panel.config(cfg);
        }

        // ── Backlight ─────────────────────────────────────────────────────
        {
            auto cfg        = _backlight.config();
            cfg.pin_bl      = TFT_BL;     // 21
            cfg.invert      = false;       // HIGH = backlight on
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _backlight.config(cfg);
            _panel.setLight(&_backlight);
        }

        // ── Touch (SPI3 / VSPI) ───────────────────────────────────────────
        {
            auto cfg             = _touch.config();
            // Raw ADC calibration limits — update after calibration sketch
            cfg.x_min            = TOUCH_X_MIN;
            cfg.x_max            = TOUCH_X_MAX;
            cfg.y_min            = TOUCH_Y_MIN;
            cfg.y_max            = TOUCH_Y_MAX;
            cfg.pin_int          = TCH_IRQ;   // 36
            cfg.bus_shared       = false;     // dedicated SPI3 bus
            cfg.offset_rotation  = 0;
            cfg.spi_host         = SPI3_HOST; // VSPI — GPIO matrix routed
            cfg.freq             = 2500000;
            cfg.pin_sclk         = TCH_CLK;   // 25
            cfg.pin_mosi         = TCH_MOSI;  // 32
            cfg.pin_miso         = TCH_MISO;  // 39
            cfg.pin_cs           = TCH_CS;    // 33
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }

        setPanel(&_panel);
    }
};

extern LGFX gfx;
void display_init();