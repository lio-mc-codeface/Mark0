#ifndef MODE_SAMPLER_H
#define MODE_SAMPLER_H

#include <Adafruit_SSD1306.h>
#include "config.h"

// 32768 samples = ~0.74s at 44.1kHz (64 KB dynamic Heap allocation)
#define SAMPLE_BUFFER_SIZE 32768

void sampler_init();
void sampler_ui_render(Adafruit_SSD1306 &display);
void sampler_audio_process(int16_t *buffer, uint16_t touchMask);

#endif