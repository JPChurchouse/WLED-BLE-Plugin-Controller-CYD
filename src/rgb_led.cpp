#include "rgb_led.hpp"
#include "config.hpp"
#include <Arduino.h>

void rgb_led_init() {
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);
    rgb_led_set(LedColor::OFF);
}

void rgb_led_set(LedColor color) {
    // Active-low: LOW = illuminated, HIGH = off
    bool r = false, g = false, b = false;
    switch (color) {
        case LedColor::RED:   r = true; break;
        case LedColor::GREEN: g = true; break;
        case LedColor::BLUE:  b = true; break;
        default: break;
    }
    digitalWrite(LED_R, r ? LOW : HIGH);
    digitalWrite(LED_G, g ? LOW : HIGH);
    digitalWrite(LED_B, b ? LOW : HIGH);
}