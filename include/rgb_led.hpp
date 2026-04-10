#pragma once

enum class LedColor { OFF, RED, GREEN, BLUE };

void rgb_led_init();
void rgb_led_set(LedColor color);