#include <Arduino.h>
#include "display_setup.hpp"

void setup()
{
    Serial.begin(115200);
    gfx.init();
    gfx.setRotation(1);
    Serial.printf("w=%d h=%d\n", gfx.width(), gfx.height());

    gfx.fillScreen(TFT_RED);
    delay(500);
    gfx.fillScreen(TFT_GREEN);
    delay(500);
    gfx.fillScreen(TFT_BLUE);
    delay(500);
    gfx.drawRect(10, 10, 100, 50, TFT_WHITE);
    gfx.drawString("Hello", 50, 30, 2);
}

void loop()
{
    uint16_t x, y;
    if (gfx.getTouch(&x, &y))
    {
        Serial.printf("Touch: %d, %d\n", x, y);
        gfx.fillCircle(x, y, 5, TFT_YELLOW);
    }
}