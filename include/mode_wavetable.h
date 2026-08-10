#ifndef MODE_WAVETABLE_H
#define MODE_WAVETABLE_H

#include <Adafruit_SSD1306.h>
#include "config.h"

void wavetable_init();
void wavetable_ui_render(Adafruit_SSD1306 &display);
void wavetable_audio_process(int16_t *buffer, uint16_t touchMask);

#endif