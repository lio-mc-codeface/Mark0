#include "mode_flute.h"
#include <math.h>

#define ATTACK_TIME_MS   120.0f
#define VIBRATO_SPEED_HZ 5.5f
#define VIBRATO_DEPTH    0.005f
#define HARMONIC_2_GAIN  0.15f
#define HARMONIC_3_GAIN  0.05f

static Voice fluteVoices[MAX_VOICES];

void flute_init() {
    for (int i = 0; i < MAX_VOICES; i++) {
        fluteVoices[i].padIndex = -1;
        fluteVoices[i].amplitude = 0.0f;
    }
}

void flute_ui_render(Adafruit_SSD1306 &display, uint16_t touchMask) {
    static int prev_y = 42;
    const char* noteNames[12] = {
        "C4", "C#4", "D4", "D#4", "E4", "F4", 
        "F#4", "G4", "G#4", "A4", "A#4", "B4"
    };

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    float primaryFreq = 261.63f;
    bool activeFound = false;

    for (int v = 0; v < MAX_VOICES; v++) {
        if (fluteVoices[v].padIndex != -1) {
            if (!activeFound) {
                primaryFreq = fluteVoices[v].baseFreq;
                activeFound = true;
            }
            display.print(noteNames[fluteVoices[v].padIndex]);
            display.print(" ");
        }
    }

    if (!activeFound) {
        display.setTextSize(1);
        display.print("MODE: FLUTE SYNTH");
    }

    display.drawFastHLine(0, 18, 128, SSD1306_WHITE);

    if (activeFound) {
        float phaseStep = (2.0f * M_PI * primaryFreq * 2.0f) / (SAMPLE_RATE);

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            float sampleSum = 0.0f;
            int voiceCount = 0;

            for (int v = 0; v < MAX_VOICES; v++) {
                if (fluteVoices[v].padIndex != -1) {
                    float p1 = (float)x * phaseStep * (fluteVoices[v].baseFreq / primaryFreq);
                    float s1 = sinf(p1);
                    float s2 = sinf(p1 * 2.0f) * HARMONIC_2_GAIN;
                    float s3 = sinf(p1 * 3.0f) * HARMONIC_3_GAIN;

                    sampleSum += (s1 + s2 + s3) * fluteVoices[v].amplitude;
                    voiceCount++;
                }
            }

            if (voiceCount > 0) sampleSum /= voiceCount;

            int y = 42 - (int)(sampleSum * 16.0f);
            y = constrain(y, 20, 62);

            if (x > 0) {
                display.drawLine(x - 1, prev_y, x, y, SSD1306_WHITE);
            } else {
                prev_y = y;
            }
        }
    }

    display.display();
}

void flute_audio_process(int16_t *buffer, uint16_t touchMask) {
    const float attackSamples = (ATTACK_TIME_MS / 1000.0f) * SAMPLE_RATE;
    const float attackStep = 1.0f / attackSamples;

    for (uint8_t pad = 0; pad < 12; pad++) {
        if (touchMask & (1 << pad)) {
            bool playing = false;
            for (int v = 0; v < MAX_VOICES; v++) {
                if (fluteVoices[v].padIndex == pad) { playing = true; break; }
            }
            if (!playing) {
                for (int v = 0; v < MAX_VOICES; v++) {
                    if (fluteVoices[v].padIndex == -1) {
                        fluteVoices[v].padIndex = pad;
                        fluteVoices[v].baseFreq = noteFreqs[pad];
                        fluteVoices[v].phase1 = 0.0f;
                        fluteVoices[v].phase2 = 0.0f;
                        fluteVoices[v].phase3 = 0.0f;
                        fluteVoices[v].vibratoPhase = 0.0f;
                        fluteVoices[v].amplitude = 0.01f;
                        break;
                    }
                }
            }
        }
    }

    for (int v = 0; v < MAX_VOICES; v++) {
        if (fluteVoices[v].padIndex != -1 && !(touchMask & (1 << fluteVoices[v].padIndex))) {
            fluteVoices[v].padIndex = -1;
            fluteVoices[v].amplitude = 0.0f;
        }
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        float mixedSample = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            if (fluteVoices[v].padIndex != -1) {
                if (fluteVoices[v].amplitude < 1.0f) {
                    fluteVoices[v].amplitude += (1.05f - fluteVoices[v].amplitude) * attackStep * 4.0f;
                    if (fluteVoices[v].amplitude > 1.0f) fluteVoices[v].amplitude = 1.0f;
                }

                float vibrato = sinf(fluteVoices[v].vibratoPhase) * VIBRATO_DEPTH;
                fluteVoices[v].vibratoPhase += (2.0f * M_PI * VIBRATO_SPEED_HZ) / SAMPLE_RATE;
                if (fluteVoices[v].vibratoPhase >= 2.0f * M_PI) fluteVoices[v].vibratoPhase -= 2.0f * M_PI;

                float modulatedFreq = fluteVoices[v].baseFreq * (1.0f + vibrato);
                float phaseInc1 = (2.0f * M_PI * modulatedFreq) / SAMPLE_RATE;

                float s1 = sinf(fluteVoices[v].phase1);
                float s2 = sinf(fluteVoices[v].phase2) * HARMONIC_2_GAIN;
                float s3 = sinf(fluteVoices[v].phase3) * HARMONIC_3_GAIN;

                mixedSample += (s1 + s2 + s3) * fluteVoices[v].amplitude;

                fluteVoices[v].phase1 += phaseInc1;
                if (fluteVoices[v].phase1 >= 2.0f * M_PI) fluteVoices[v].phase1 -= 2.0f * M_PI;
                fluteVoices[v].phase2 += phaseInc1 * 2.0f;
                if (fluteVoices[v].phase2 >= 2.0f * M_PI) fluteVoices[v].phase2 -= 2.0f * M_PI;
                fluteVoices[v].phase3 += phaseInc1 * 3.0f;
                if (fluteVoices[v].phase3 >= 2.0f * M_PI) fluteVoices[v].phase3 -= 2.0f * M_PI;
            }
        }

        int16_t sample = (int16_t)(mixedSample * (MASTER_VOLUME / MAX_VOICES));
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}