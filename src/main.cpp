#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/i2s.h"
#include <math.h>

// I2S Hardware Pins (MAX98357A)
#define I2S_NUM         I2S_NUM_0
#define I2S_BCK_PIN     26
#define I2S_LRC_PIN     25
#define I2S_DOUT_PIN    17

// OLED Display Configuration
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS    0x3C // Common SSD1306 address (or 0x3D)

// Touch Sensor Configuration
#define TOUCH_THRESHOLD 12

// Audio Hardware Setup
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     128
#define MASTER_VOLUME   6000
#define MAX_VOICES      4

// --- SYNTHESIZER & FLUTE ENVELOPE PARAMETERS ---
#define ATTACK_TIME_MS   120.0f  // Time (ms) to ramp up from silence to full volume
#define VIBRATO_SPEED_HZ 5.5f    // Pitch modulation speed
#define VIBRATO_DEPTH    0.005f  // Pitch modulation depth
#define HARMONIC_2_GAIN  0.15f   // 2nd harmonic (octave above)
#define HARMONIC_3_GAIN  0.05f   // 3rd harmonic (octave + 5th)

const float noteFreqs[12] = {
    261.63, 277.18, 293.66, 311.13,
    329.63, 349.23, 369.99, 392.00,
    415.30, 440.00, 466.16, 493.88
};

struct Voice {
    int padIndex = -1;
    float baseFreq = 0.0f;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;
    float vibratoPhase = 0.0f;
    float amplitude = 0.0f;
};

Voice voices[MAX_VOICES];

// Shared touch mask between Core 0 and Core 1
volatile uint16_t activeTouchMask = 0;

Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int16_t audio_buffer[BUFFER_SIZE * 2];
int16_t silence_buffer[BUFFER_SIZE * 2] = {0};

void init_i2s() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
}

// --- TASK 1: RUNS ON CORE 0 (Touch Scanning & Waveform Display) ---
void uiTask(void *pvParameters) {
    int prev_y = 42;

    const char* noteNames[12] = {
        "C4", "C#4", "D4", "D#4", "E4", "F4", 
        "F#4", "G4", "G#4", "A4", "A#4", "B4"
    };

    while (1) {
        // 1. Touch Read
        uint16_t mask = 0;
        for (uint8_t i = 0; i < 12; i++) {
            uint16_t val = cap.filteredData(i);
            if (val < TOUCH_THRESHOLD && val > 0) {
                mask |= (1 << i);
            }
        }
        activeTouchMask = mask;

        // 2. Waveform Visualizer Rendering
        display.clearDisplay();

        // Check active voices
        float primaryFreq = 261.63f;
        bool activeFound = false;

        // Larger Text Size for Note Names
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);

        // Header status bar
        int activeCount = 0;
        for (int v = 0; v < MAX_VOICES; v++) {
            if (voices[v].padIndex != -1) {
                if (!activeFound) {
                    primaryFreq = voices[v].baseFreq;
                    activeFound = true;
                }
                display.print(noteNames[voices[v].padIndex]);
                display.print(" ");
                activeCount++;
            }
        }

        if (!activeFound) {
            display.setTextSize(2);
            display.print("FLUTE");
        }

        display.drawFastHLine(0, 18, 128, SSD1306_WHITE); // Divider line below text header

        if (activeFound) {
            // Plot composite wave across 128 screen pixels
            float phaseStep = (2.0f * M_PI * primaryFreq * 2.0f) / (SAMPLE_RATE);

            for (int x = 0; x < SCREEN_WIDTH; x++) {
                float sampleSum = 0.0f;
                int voiceCount = 0;

                for (int v = 0; v < MAX_VOICES; v++) {
                    if (voices[v].padIndex != -1) {
                        float p1 = (float)x * phaseStep * (voices[v].baseFreq / primaryFreq);
                        float s1 = sinf(p1);
                        float s2 = sinf(p1 * 2.0f) * HARMONIC_2_GAIN;
                        float s3 = sinf(p1 * 3.0f) * HARMONIC_3_GAIN;

                        sampleSum += (s1 + s2 + s3) * voices[v].amplitude;
                        voiceCount++;
                    }
                }

                if (voiceCount > 0) {
                    sampleSum /= voiceCount;
                }

                // Map wave below the larger text header (Y: 20 to 62)
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
        vTaskDelay(pdMS_TO_TICKS(30)); // ~33 FPS refresh rate
    }
}

// --- TASK 2: RUNS ON CORE 1 (Polyphonic Flute Audio Engine) ---
void audioTask(void *pvParameters) {
    size_t bytes_written;
    const float attackSamples = (ATTACK_TIME_MS / 1000.0f) * SAMPLE_RATE;
    const float attackStep = 1.0f / attackSamples;

    while (1) {
        uint16_t currentMask = activeTouchMask;

        // Voice Allocation
        for (uint8_t pad = 0; pad < 12; pad++) {
            if (currentMask & (1 << pad)) {
                bool alreadyPlaying = false;
                for (int v = 0; v < MAX_VOICES; v++) {
                    if (voices[v].padIndex == pad) {
                        alreadyPlaying = true;
                        break;
                    }
                }

                if (!alreadyPlaying) {
                    for (int v = 0; v < MAX_VOICES; v++) {
                        if (voices[v].padIndex == -1) {
                            voices[v].padIndex = pad;
                            voices[v].baseFreq = noteFreqs[pad];
                            voices[v].phase1 = 0.0f;
                            voices[v].phase2 = 0.0f;
                            voices[v].phase3 = 0.0f;
                            voices[v].vibratoPhase = 0.0f;
                            voices[v].amplitude = 0.01f;
                            break;
                        }
                    }
                }
            }
        }

        // Voice Release
        for (int v = 0; v < MAX_VOICES; v++) {
            if (voices[v].padIndex != -1) {
                if (!(currentMask & (1 << voices[v].padIndex))) {
                    voices[v].padIndex = -1;
                    voices[v].amplitude = 0.0f;
                }
            }
        }

        // Audio Buffer Generation
        bool activeSound = false;

        for (int i = 0; i < BUFFER_SIZE; i++) {
            float mixedSample = 0.0f;

            for (int v = 0; v < MAX_VOICES; v++) {
                if (voices[v].padIndex != -1) {
                    activeSound = true;

                    // Logarithmic Attack
                    if (voices[v].amplitude < 1.0f) {
                        voices[v].amplitude += (1.05f - voices[v].amplitude) * attackStep * 4.0f;
                        if (voices[v].amplitude > 1.0f) voices[v].amplitude = 1.0f;
                    }

                    // Vibrato
                    float vibrato = sinf(voices[v].vibratoPhase) * VIBRATO_DEPTH;
                    voices[v].vibratoPhase += (2.0f * M_PI * VIBRATO_SPEED_HZ) / SAMPLE_RATE;
                    if (voices[v].vibratoPhase >= 2.0f * M_PI) voices[v].vibratoPhase -= 2.0f * M_PI;

                    float modulatedFreq = voices[v].baseFreq * (1.0f + vibrato);

                    // Harmonics
                    float phaseInc1 = (2.0f * M_PI * modulatedFreq) / SAMPLE_RATE;
                    float phaseInc2 = phaseInc1 * 2.0f;
                    float phaseInc3 = phaseInc1 * 3.0f;

                    float s1 = sinf(voices[v].phase1);
                    float s2 = sinf(voices[v].phase2) * HARMONIC_2_GAIN;
                    float s3 = sinf(voices[v].phase3) * HARMONIC_3_GAIN;

                    float voiceSample = (s1 + s2 + s3) * voices[v].amplitude;
                    mixedSample += voiceSample;

                    voices[v].phase1 += phaseInc1;
                    if (voices[v].phase1 >= 2.0f * M_PI) voices[v].phase1 -= 2.0f * M_PI;

                    voices[v].phase2 += phaseInc2;
                    if (voices[v].phase2 >= 2.0f * M_PI) voices[v].phase2 -= 2.0f * M_PI;

                    voices[v].phase3 += phaseInc3;
                    if (voices[v].phase3 >= 2.0f * M_PI) voices[v].phase3 -= 2.0f * M_PI;
                }
            }

            int16_t finalSample = (int16_t)(mixedSample * (MASTER_VOLUME / MAX_VOICES));
            audio_buffer[i * 2]     = finalSample;
            audio_buffer[i * 2 + 1] = finalSample;
        }

        if (activeSound) {
            i2s_write(I2S_NUM, audio_buffer, sizeof(audio_buffer), &bytes_written, portMAX_DELAY);
        } else {
            i2s_write(I2S_NUM, silence_buffer, sizeof(silence_buffer), &bytes_written, portMAX_DELAY);
        }
    }
}

void setup() {
    Serial.begin(115200);

    Wire.begin(21, 22);
    Wire.setClock(400000); // Fast 400 kHz I2C

    // OLED Init
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("Error: SSD1306 not detected!");
    } else {
        display.setRotation(2); // Flips the display 180 degrees
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(2);
        display.setCursor(10, 24);
        display.print("Flute Synth");
        display.display();
        delay(500);
    }

    // MPR121 Init
    if (!cap.begin(0x5A, &Wire)) {
        Serial.println("Error: MPR121 not found at 0x5A!");
        while (1);
    }

    init_i2s();

    // Core 0: Touch + Display Task
    xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);

    // Core 1: High Priority Audio Generation
    xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}