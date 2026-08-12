#include "mode_flute.h"
#include <Arduino.h>
#include <math.h>

static Voice fluteVoices[MAX_VOICES];

void flute_init() {
    for (int i = 0; i < MAX_VOICES; i++) {
        fluteVoices[i].padIndex = -1;
        fluteVoices[i].baseFreq = 0.0f;
        fluteVoices[i].phase1 = 0.0f;
        fluteVoices[i].phase2 = 0.0f;
        fluteVoices[i].phase3 = 0.0f;
        fluteVoices[i].vibratoPhase = 0.0f;
        fluteVoices[i].amplitude = 0.0f;
    }
}

void flute_ui_render(Adafruit_SSD1306 &display, uint16_t touchMask) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("MODE: FLUTE");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Active pad indicators
    display.setCursor(0, 16);
    display.print("Pads: ");
    for (uint8_t i = 0; i < 12; i++) {
        if (touchMask & (1 << i)) {
            display.printf("%d ", i);
        }
    }

    // Active voices count
    int activeCount = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (fluteVoices[i].padIndex != -1) activeCount++;
    }

    display.setCursor(0, 36);
    display.printf("Active Voices: %d/%d", activeCount, MAX_VOICES);

    display.setCursor(0, 56);
    display.print("Touch Pads 0-11 to play");
    display.display();
}

void flute_audio_process(int16_t *buffer, uint16_t touchMask) {
    // 1. Voice Allocation (Trigger notes on touch)
    for (uint8_t pad = 0; pad < 12; pad++) {
        if (touchMask & (1 << pad)) {
            bool alreadyPlaying = false;
            for (int v = 0; v < MAX_VOICES; v++) {
                if (fluteVoices[v].padIndex == pad) {
                    alreadyPlaying = true;
                    break;
                }
            }

            if (!alreadyPlaying) {
                for (int v = 0; v < MAX_VOICES; v++) {
                    if (fluteVoices[v].padIndex == -1) {
                        fluteVoices[v].padIndex = pad;
                        fluteVoices[v].baseFreq = noteFreqs[pad];
                        fluteVoices[v].phase1 = 0.0f;
                        fluteVoices[v].phase2 = 0.0f;
                        fluteVoices[v].phase3 = 0.0f;
                        fluteVoices[v].vibratoPhase = 0.0f;
                        fluteVoices[v].amplitude = 1.0f;
                        break;
                    }
                }
            }
        }
    }

    // 2. Voice Release (Release notes when touch ends)
    for (int v = 0; v < MAX_VOICES; v++) {
        if (fluteVoices[v].padIndex != -1) {
            uint8_t pad = fluteVoices[v].padIndex;
            if (!(touchMask & (1 << pad))) {
                fluteVoices[v].padIndex = -1;
                fluteVoices[v].amplitude = 0.0f;
            }
        }
    }

    // 3. Flute Audio Synthesis DSP Loop
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float mixedSample = 0.0f;
        int activeVoiceCount = 0;

        for (int v = 0; v < MAX_VOICES; v++) {
            if (fluteVoices[v].amplitude > 0.001f) {
                activeVoiceCount++;

                // Vibrato (Frequency Modulation)
                float vibratoLFO = sinf(fluteVoices[v].vibratoPhase);
                fluteVoices[v].vibratoPhase += (2.0f * M_PI * LFO_RATE_HZ) / SAMPLE_RATE;
                if (fluteVoices[v].vibratoPhase >= 2.0f * M_PI) {
                    fluteVoices[v].vibratoPhase -= 2.0f * M_PI;
                }

                float freqMod = fluteVoices[v].baseFreq * powf(2.0f, (vibratoLFO * VIBRATO_DEPTH_SEMITONES) / 12.0f);

                // Harmonics for flute timbre
                float step1 = (2.0f * M_PI * freqMod) / SAMPLE_RATE;
                float step2 = (2.0f * M_PI * (freqMod * 2.0f)) / SAMPLE_RATE;
                float step3 = (2.0f * M_PI * (freqMod * 3.0f)) / SAMPLE_RATE;

                fluteVoices[v].phase1 += step1;
                fluteVoices[v].phase2 += step2;
                fluteVoices[v].phase3 += step3;

                if (fluteVoices[v].phase1 >= 2.0f * M_PI) fluteVoices[v].phase1 -= 2.0f * M_PI;
                if (fluteVoices[v].phase2 >= 2.0f * M_PI) fluteVoices[v].phase2 -= 2.0f * M_PI;
                if (fluteVoices[v].phase3 >= 2.0f * M_PI) fluteVoices[v].phase3 -= 2.0f * M_PI;

                float sampleVal = sinf(fluteVoices[v].phase1) * 0.70f +
                                  sinf(fluteVoices[v].phase2) * 0.20f +
                                  sinf(fluteVoices[v].phase3) * 0.10f;

                // Tremolo (Amplitude Modulation)
                float tremoloAmp = 1.0f - (TREMOLO_DEPTH * 0.5f * (1.0f + vibratoLFO));

                mixedSample += sampleVal * fluteVoices[v].amplitude * tremoloAmp;
            }
        }

        // Apply headroom division based on MAX_VOICES to avoid integer wrap-around distortion
        float gainScale = (float)MASTER_VOLUME / (float)MAX_VOICES;
        float scaledVal = mixedSample * gainScale;

        // Hard clipping protection guard
        int16_t finalSample = (int16_t)constrain(scaledVal, -32767.0f, 32767.0f);

        buffer[i * 2]     = finalSample; // Left Channel
        buffer[i * 2 + 1] = finalSample; // Right Channel
    }
}