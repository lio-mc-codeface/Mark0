#ifndef MODE_FLUTE_H
#define MODE_FLUTE_H

#include <Adafruit_SSD1306.h>
#include "config.h"

void flute_init();
void flute_ui_render(Adafruit_SSD1306 &display, uint16_t touchMask);
void flute_audio_process(int16_t *buffer, uint16_t touchMask);

#endif