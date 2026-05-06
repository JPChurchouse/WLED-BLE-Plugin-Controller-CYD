#include <Arduino.h>
#include "config.hpp"
#include "rgb_led.hpp"
#include "display_setup.hpp"
#include "ble_client.hpp"
#include "ui_manager.hpp"
#include <lvgl.h>

static uint32_t  s_lastTickMs = 0;
static BLEStatus s_prevStatus = BLEStatus::IDLE;

void setup()
{
    Serial.begin(115200);
    Serial.println("\n[MAIN] Boot");

    rgb_led_init();
    rgb_led_set(LedColor::BLUE);

    display_init();
    ui_init();

    // Force LVGL to repaint every pixel on the connecting screen right now.
    // lv_scr_load() marks the screen dirty but does not flush immediately;
    // lv_refr_now() blocks until the full screen has been pushed to the panel.
    // This overwrites any GRAM content left by previous firmware.
    lv_refr_now(lv_disp_get_default());

    ble_init();

    s_lastTickMs = millis();
    Serial.println("[MAIN] Setup complete");
}

void loop()
{
    uint32_t now     = millis();
    uint32_t elapsed = now - s_lastTickMs;
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        s_lastTickMs = now;
    }

    lv_timer_handler();
    ui_apply_updates();

    // RGB LED status indicator
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

    delay(5);
}