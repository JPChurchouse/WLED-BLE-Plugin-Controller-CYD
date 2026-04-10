#pragma once
#include <cstdint>

// ── BLE ────────────────────────────────────────────────────────────────────
constexpr char     BLE_TARGET_NAME[]       = "WLED-BLE";
constexpr char     BLE_SVC_UUID[]          = "4fafc201-1fb5-459e-8fcc-c5c9c3319100";
constexpr char     BLE_CHR_POWER_UUID[]    = "4fafc202-1fb5-459e-8fcc-c5c9c3319100";
constexpr char     BLE_CHR_PRESETS_UUID[]  = "4fafc203-1fb5-459e-8fcc-c5c9c3319100";
constexpr char     BLE_CHR_ACTIVE_UUID[]   = "4fafc204-1fb5-459e-8fcc-c5c9c3319100";

constexpr uint16_t BLE_MTU                 = 517;
constexpr uint32_t BLE_RECONNECT_DELAY_MS  = 2000;   // pause before rescanning
constexpr uint32_t BLE_TASK_STACK_BYTES    = 8192;
constexpr uint8_t  BLE_TASK_CORE           = 0;      // BLE stack lives on core 0
constexpr uint8_t  BLE_TASK_PRIORITY       = 2;

// ── Display — ILI9341 on HSPI (SPI2) ─────────────────────────────────────
constexpr int TFT_MOSI = 13;
constexpr int TFT_MISO = 12;
constexpr int TFT_CLK  = 14;
constexpr int TFT_CS   = 15;
constexpr int TFT_DC   =  2;
constexpr int TFT_BL   = 21;

// ── Touch — XPT2046 on VSPI (SPI3) ───────────────────────────────────────
constexpr int TCH_MOSI = 32;
constexpr int TCH_MISO = 39;
constexpr int TCH_CLK  = 25;
constexpr int TCH_CS   = 33;
constexpr int TCH_IRQ  = 36;

// XPT2046 raw ADC calibration — run calibration routine first, paste here
// These values map raw touch ADC → screen pixels.
// Typical starting values; adjust after running the calibration sketch.
constexpr int TOUCH_X_MIN = 280;
constexpr int TOUCH_X_MAX = 3800;
constexpr int TOUCH_Y_MIN = 280;
constexpr int TOUCH_Y_MAX = 3800;

// ── RGB LED (active-low) ──────────────────────────────────────────────────
constexpr int LED_R = 17;
constexpr int LED_G = 16;
constexpr int LED_B =  4;

// ── LVGL draw buffer ─────────────────────────────────────────────────────
constexpr int DISP_WIDTH    = 320;
constexpr int DISP_HEIGHT   = 240;
constexpr int LVGL_BUF_LINES = 30;  // double-buffered: 2 × (320 × 30 × 2) = ~38 KB