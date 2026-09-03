#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <Adafruit_ST7789.h>

// Color helper
uint16_t interpolateColor(uint16_t startColor, uint16_t endColor, float t) {
    uint8_t r1 = (startColor >> 11) & 0x1F;
    uint8_t g1 = (startColor >> 5) & 0x3F;
    uint8_t b1 = startColor & 0x1F;

    uint8_t r2 = (endColor >> 11) & 0x1F;
    uint8_t g2 = (endColor >> 5) & 0x3F;
    uint8_t b2 = endColor & 0x1F;

    uint8_t r = r1 + (uint8_t)((r2 - r1) * t);
    uint8_t g = g1 + (uint8_t)((g2 - g1) * t);
    uint8_t b = b1 + (uint8_t)((b2 - b1) * t);

    return (r << 11) | (g << 5) | b;
}

void run_boot_animation(Adafruit_ST7789 &tft) {
    uint16_t yellow = 0xFDE0;
    uint16_t purple = 0x8010;
    
    int w = tft.width();
    int h = tft.height();
    int cx = w / 2;
    int cy = h / 2;

    // 1. Color fade (approx 500ms)
    int fade_steps = 20;
    for (int i = 0; i <= fade_steps; i++) {
        float t = (float)i / fade_steps;
        tft.fillScreen(interpolateColor(yellow, purple, t));
        delay(25);
    }

}

#endif
