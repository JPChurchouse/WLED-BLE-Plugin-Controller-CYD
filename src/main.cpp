#include <Arduino.h>
#include "config.hpp"
#include "rgb_led.hpp"
#include "display_setup.hpp"
#include "ble_client.hpp"
#include "ui_manager.hpp"
#include <lvgl.h>

// Arduino loop() runs on core 1 — LVGL and UI live here.
// The BLE management task is pinned to core 0 (see ble_client.cpp).

static uint32_t s_lastTickMs = 0;
static BLEStatus s_prevStatus = BLEStatus::IDLE;

void setup() {
    Serial.begin(115200);
    Serial.println("\n[MAIN] Boot");

    rgb_led_init();
    rgb_led_set(LedColor::BLUE);   // visual indication: scanning

    display_init();                // LovyanGFX + LVGL driver registration
    ui_init();                     // build LVGL screens

    // Small yield so LVGL renders the connecting screen before BLE starts
    lv_task_handler();
    delay(50);

    ble_init();                    // spawns ble_task on core 0, begins scan

    s_lastTickMs = millis();
    Serial.println("[MAIN] Setup complete");
}

void loop() {
    // ── LVGL tick source ──────────────────────────────────────────────────
    // lv_tick_inc() must be called every millisecond (or in chunks).
    // We accumulate elapsed ms and pass the delta each iteration.
    uint32_t now     = millis();
    uint32_t elapsed = now - s_lastTickMs;
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        s_lastTickMs = now;
    }

    // ── LVGL timer handler ────────────────────────────────────────────────
    // Processes animations, input events, and triggers flush callbacks.
    lv_timer_handler();

    // ── BLE → UI bridge ───────────────────────────────────────────────────
    ui_apply_updates();

    // ── RGB LED status ────────────────────────────────────────────────────
    BLEStatus st = BLEStatus::IDLE;
    if (xSemaphoreTake(g_ble.mutex, 0) == pdTRUE) {
        st = g_ble.status;
        xSemaphoreGive(g_ble.mutex);
    }
    if (st != s_prevStatus) {
        s_prevStatus = st;
        switch (st) {
            case BLEStatus::SCANNING:
            case BLEStatus::CONNECTING:   rgb_led_set(LedColor::BLUE);  break;
            case BLEStatus::CONNECTED:    rgb_led_set(LedColor::GREEN); break;
            case BLEStatus::DISCONNECTED: rgb_led_set(LedColor::RED);   break;
            default:                      rgb_led_set(LedColor::OFF);   break;
        }
    }

    // ── Yield ─────────────────────────────────────────────────────────────
    // 5 ms gives ~200 Hz loop rate. lv_timer_handler() already has internal
    // rate limiting so this just prevents busy-spinning on the idle RTOS tick.
    delay(5);
}