#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_ST7789.h>
#include "driver/i2s.h"
#include <math.h>

#include "config.h"
#include "boot_logo.h"
#include "boot_screen.h"
#include "mode_flute.h"
#include "mode_wavetable.h"
#include "mode_sampler.h"

Envelope globalEnvelope;

void updateEnvelopeRates() {
    float aTime = max(globalEnvelope.attackTimeSeconds, 0.001f);
    float dTime = max(globalEnvelope.decayTimeSeconds, 0.001f);
    float rTime = max(globalEnvelope.releaseTimeSeconds, 0.001f);

    globalEnvelope.attackRate = 1.0f / (aTime * SAMPLE_RATE);
    globalEnvelope.decayRate = (1.0f - globalEnvelope.sustainLevel) / (dTime * SAMPLE_RATE);
    globalEnvelope.releaseRate = globalEnvelope.sustainLevel / (rTime * SAMPLE_RATE);
}

void setEnvelopeParameters(float a, float d, float s, float r) {
    globalEnvelope.attackTimeSeconds = a;
    globalEnvelope.decayTimeSeconds = d;
    globalEnvelope.sustainLevel = constrain(s, 0.0f, 1.0f);
    globalEnvelope.releaseTimeSeconds = r;
    updateEnvelopeRates();
}

void toggleEnvelope() {
    globalEnvelope.enabled = !globalEnvelope.enabled;
    Serial.print("Global Envelope: ");
    Serial.println(globalEnvelope.enabled ? "ON" : "OFF");
    if (globalEnvelope.enabled) {
        Serial.print(" -> A: "); Serial.print(globalEnvelope.attackTimeSeconds, 4); Serial.println("s");
        Serial.print(" -> D: "); Serial.print(globalEnvelope.decayTimeSeconds, 4); Serial.println("s");
        Serial.print(" -> S: "); Serial.print(globalEnvelope.sustainLevel, 2); Serial.println("");
        Serial.print(" -> R: "); Serial.print(globalEnvelope.releaseTimeSeconds, 4); Serial.println("s");
    }
}

volatile SynthMode currentMode = MODE_FLUTE;

volatile uint16_t activeTouchMask = 0;
volatile uint8_t analogButtonState = 0;
uint16_t restingCapacitance[12] = {0};

Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_GC9A01A tft(TFT1_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
Adafruit_ST7789 tft2(TFT2_CS_PIN, TFT_DC_PIN, TFT2_RST_PIN);

volatile int16_t visualAudio[BUFFER_SIZE] = {};
portMUX_TYPE visualAudioMux = portMUX_INITIALIZER_UNLOCKED;

void init_gpio() {
    pinMode(MPR121_IRQ_PIN, INPUT_PULLUP);

    pinMode(TFT2_CS_PIN, OUTPUT);
    digitalWrite(TFT2_CS_PIN, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
}

void init_i2s_dac() {
    i2s_config_t dac_i2s_config = {
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

    i2s_pin_config_t dac_pin_config = {
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM, &dac_i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &dac_pin_config);
    i2s_zero_dma_buffer(I2S_NUM);
}

void init_mems_mic() {
    i2s_config_t mic_i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t mic_pin_config = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD
    };

    i2s_driver_uninstall(I2S_MIC_NUM);

    esp_err_t err = i2s_driver_install(I2S_MIC_NUM, &mic_i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Mic I2S Driver Install Failed: 0x%X\n", err);
        return;
    }

    err = i2s_set_pin(I2S_MIC_NUM, &mic_pin_config);
    if (err != ESP_OK) {
        Serial.printf("Mic I2S Set Pin Failed: 0x%X\n", err);
        return;
    }

    i2s_zero_dma_buffer(I2S_MIC_NUM);
    Serial.println("I2S MEMS Mic Initialized Successfully!");
}

void init_gc9a01() {
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(10);
    digitalWrite(TFT_RST_PIN, LOW);
    delay(50);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(120);

    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(GC9A01A_BLACK);
    tft.drawRGBBitmap(0, 0, bootLogo, 240, 240);
}

void init_st7789() {
    tft2.init(240, 280);
    tft2.setRotation(1);
    tft2.invertDisplay(true);
    tft2.fillScreen(ST77XX_BLACK);
    run_boot_animation(tft2);
}

void draw_scope_trace(const int16_t *samples, uint16_t color) {
    for (int x = 1; x < 240; x++) {
        int first = ((x - 1) * (BUFFER_SIZE - 1)) / 239;
        int second = (x * (BUFFER_SIZE - 1)) / 239;
        int y1 = 120 - constrain((int)samples[first] / 96, -95, 95);
        int y2 = 120 - constrain((int)samples[second] / 96, -95, 95);
        int dx1 = x - 1 - 120;
        int dx2 = x - 120;
        int dy1 = y1 - 120;
        int dy2 = y2 - 120;
        if (dx1 * dx1 + dy1 * dy1 <= 115 * 115 &&
            dx2 * dx2 + dy2 * dy2 <= 115 * 115) {
            tft.drawLine(x - 2, y1, x + 1, y2, color);
            tft.drawLine(x - 1, y1 - 1, x, y2 - 1, color);
            tft.drawLine(x - 1, y1 + 1, x, y2 + 1, color);
        }
    }
}

void update_live_displays() {
    int16_t samples[BUFFER_SIZE];
    static int16_t previousSamples[BUFFER_SIZE] = {};
    static bool scopeInitialized = false;

    portENTER_CRITICAL(&visualAudioMux);
    memcpy(samples, (const void *)visualAudio, sizeof(samples));
    portEXIT_CRITICAL(&visualAudioMux);

    // Round GC9A01 scope
    if (!scopeInitialized) {
        tft.fillScreen(GC9A01A_BLACK);
        for (int ring = 0; ring < 5; ring++) {
            float progress = (float)ring / 4.0f;
            uint8_t red = (uint8_t)(255.0f - (127.0f * progress));
            uint8_t green = (uint8_t)(215.0f - (215.0f * progress));
            uint8_t blue = (uint8_t)(128.0f * progress);
            tft.drawCircle(120, 120, 119 - ring, tft.color565(red, green, blue));
        }
        scopeInitialized = true;
    } else {
        draw_scope_trace(previousSamples, GC9A01A_BLACK);
    }

    draw_scope_trace(samples, GC9A01A_CYAN);
    memcpy(previousSamples, samples, sizeof(previousSamples));

    // ST7789 Spectrum
    const int barCount = 12;
    const int barWidth = 8;
    const int barGap = 4;
    const int edgeSize = 8;
    const int barHeight = tft2.height() - (edgeSize * 2) - 20;
    const int bottom = tft2.height() - edgeSize - 1;
    const float spectrumScale = 20.0f;
    const float spectrumVisualGain = 1.6f;
    const float spectrumFrequencies[barCount] = {
        60.0f, 100.0f, 160.0f, 250.0f,
        375.0f, 550.0f, 800.0f, 1150.0f,
        1650.0f, 2350.0f, 3300.0f, 4750.0f
    };

    static bool spectrumCleared = false;
    if (!spectrumCleared) {
        tft2.fillScreen(ST77XX_BLACK);
    }

    for (int bar = 0; bar < barCount; bar++) {
        float frequency = spectrumFrequencies[bar];
        float real = 0.0f, imaginary = 0.0f;
        for (int sample = 0; sample < BUFFER_SIZE; sample++) {
            float angle = (2.0f * M_PI * frequency * sample) / SAMPLE_RATE;
            real += samples[sample] * cosf(angle);
            imaginary -= samples[sample] * sinf(angle);
        }
        float magnitude = sqrtf(real * real + imaginary * imaginary) / BUFFER_SIZE;
        int height = constrain((int)(magnitude * spectrumVisualGain / spectrumScale), 0, barHeight);
        int x = (tft2.width() - (barCount * (barWidth + barGap) - barGap)) / 2
              + bar * (barWidth + barGap);

        int greenHeight = min(height, (barHeight * 70) / 100);
        int yellowHeight = min(max(height - greenHeight, 0), (barHeight * 20) / 100);
        int redHeight = max(height - greenHeight - yellowHeight, 0);

        tft2.fillRect(x, bottom - barHeight + 1, barWidth, barHeight, ST77XX_BLACK);
        if (greenHeight > 0)
            tft2.fillRect(x, bottom - greenHeight + 1, barWidth, greenHeight, ST77XX_GREEN);
        if (yellowHeight > 0)
            tft2.fillRect(x, bottom - greenHeight - yellowHeight + 1, barWidth, yellowHeight, ST77XX_YELLOW);
        if (redHeight > 0)
            tft2.fillRect(x, bottom - height + 1, barWidth, redHeight, ST77XX_RED);
    }

    if (!spectrumCleared) {
        for (int edge = 0; edge < 5; edge++) {
            float progress = (float)edge / 4.0f;
            uint8_t topRed = (uint8_t)(255.0f - (127.0f * progress));
            uint8_t topGreen = (uint8_t)(215.0f - (215.0f * progress));
            uint8_t topBlue = (uint8_t)(128.0f * progress);
            uint8_t bottomRed = (uint8_t)(128.0f + (127.0f * progress));
            uint8_t bottomGreen = (uint8_t)(215.0f * progress);
            uint8_t bottomBlue = (uint8_t)(128.0f - (128.0f * progress));
            tft2.drawFastHLine(0, edge, tft2.width(), tft2.color565(topRed, topGreen, topBlue));
            tft2.drawFastHLine(0, tft2.height() - 1 - edge, tft2.width(), tft2.color565(bottomRed, bottomGreen, bottomBlue));
        }
        spectrumCleared = true;
    }
}

uint8_t read_debounced_buttons() {
    static uint8_t stableState = 0;
    static uint8_t candidateState = 0;
    static unsigned long candidateSince = 0;

    int millivolts = analogReadMilliVolts(BUTTON_ADC_PIN);
    uint8_t currentState;
    if (millivolts < BUTTON_BOTH_MAX_MV) {
        currentState = 3;
    } else if (millivolts < BUTTON1_MAX_MV) {
        currentState = 1;
    } else if (millivolts < BUTTON2_MAX_MV) {
        currentState = 2;
    } else {
        currentState = 0;
    }

    if (currentState != candidateState) {
        candidateState = currentState;
        candidateSince = millis();
    } else if (currentState != stableState &&
               millis() - candidateSince >= BUTTON_DEBOUNCE_MS) {
        stableState = currentState;
    }

    return stableState;
}

void uiTask(void *pvParameters) {
    unsigned long pad11PressStart = 0;
    bool pad11Handled = false;
    unsigned long pad7PressStart = 0;
    bool pad7Handled = false;

    vTaskDelay(pdMS_TO_TICKS(100));
    for (uint8_t i = 0; i < 12; i++) {
        restingCapacitance[i] = cap.filteredData(i);
    }

    while (1) {
        uint16_t mask = 0;

        for (uint8_t i = 0; i < 12; i++) {
            uint16_t val = cap.filteredData(i);
            if (val > 0 && restingCapacitance[i] > val) {
                uint16_t delta = restingCapacitance[i] - val;
                if (delta >= TOUCH_DELTA_THRESHOLD) {
                    mask |= (1 << i);
                }
            }
        }

        // Handle Envelope Toggle (Pad 7)
        if (mask & (1 << 7)) {
            if (pad7PressStart == 0) pad7PressStart = millis();
            else if (!pad7Handled && (millis() - pad7PressStart >= LONG_PRESS_MS)) {
                toggleEnvelope();
                pad7Handled = true;
            }
        } else {
            pad7PressStart = 0;
            pad7Handled = false;
        }

        if (mask & (1 << LONG_PRESS_PAD)) {
            if (pad11PressStart == 0) pad11PressStart = millis();
            else if (!pad11Handled && (millis() - pad11PressStart >= LONG_PRESS_MS)) {
                if (currentMode == MODE_FLUTE) {
                    currentMode = MODE_WAVETABLE;
                } else if (currentMode == MODE_WAVETABLE) {
                    currentMode = MODE_SAMPLER;
                } else {
                    currentMode = MODE_FLUTE;
                }
                pad11Handled = true;
            }
        } else {
            pad11PressStart = 0;
            pad11Handled = false;
        }

        activeTouchMask = mask;
        analogButtonState = read_debounced_buttons();

        if (currentMode == MODE_FLUTE) {
            flute_ui_render(display, activeTouchMask);
        } else if (currentMode == MODE_WAVETABLE) {
            wavetable_ui_render(display);
        } else if (currentMode == MODE_SAMPLER) {
            sampler_ui_render(display);
        }

        update_live_displays();

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void audioTask(void *pvParameters) {
    int16_t audioBuffer[BUFFER_SIZE * 2];

    while (1) {
        if (currentMode == MODE_FLUTE) {
            flute_audio_process(audioBuffer, activeTouchMask);
        } else if (currentMode == MODE_WAVETABLE) {
            wavetable_audio_process(audioBuffer, activeTouchMask);
        } else if (currentMode == MODE_SAMPLER) {
            sampler_audio_process(audioBuffer, activeTouchMask);
        }

        // Apply Global Envelope
        if (globalEnvelope.enabled) {
            bool gate = (activeTouchMask > 0);

            for (int i = 0; i < BUFFER_SIZE; i++) {
                // Check edge detection on gate transitions
                if (gate && !globalEnvelope.lastGateState) {
                    globalEnvelope.phase = ENVELOPE_ATTACK;
                } else if (!gate && globalEnvelope.lastGateState) {
                    globalEnvelope.phase = ENVELOPE_RELEASE;
                    // Recalculate release rate based on amplitude level at release onset
                    float rTime = max(globalEnvelope.releaseTimeSeconds, 0.001f);
                    globalEnvelope.releaseRate = globalEnvelope.amplitude / (rTime * SAMPLE_RATE);
                }
                globalEnvelope.lastGateState = gate;

                // State Machine Step
                switch (globalEnvelope.phase) {
                    case ENVELOPE_ATTACK:
                        globalEnvelope.amplitude += globalEnvelope.attackRate;
                        if (globalEnvelope.amplitude >= 1.0f) {
                            globalEnvelope.amplitude = 1.0f;
                            globalEnvelope.phase = ENVELOPE_DECAY;
                        }
                        break;

                    case ENVELOPE_DECAY:
                        if (globalEnvelope.amplitude > globalEnvelope.sustainLevel) {
                            globalEnvelope.amplitude -= globalEnvelope.decayRate;
                            if (globalEnvelope.amplitude <= globalEnvelope.sustainLevel) {
                                globalEnvelope.amplitude = globalEnvelope.sustainLevel;
                                globalEnvelope.phase = ENVELOPE_SUSTAIN;
                            }
                        } else {
                            globalEnvelope.amplitude += globalEnvelope.decayRate;
                            if (globalEnvelope.amplitude >= globalEnvelope.sustainLevel) {
                                globalEnvelope.amplitude = globalEnvelope.sustainLevel;
                                globalEnvelope.phase = ENVELOPE_SUSTAIN;
                            }
                        }
                        break;

                    case ENVELOPE_SUSTAIN:
                        globalEnvelope.amplitude = globalEnvelope.sustainLevel;
                        break;

                    case ENVELOPE_RELEASE:
                        globalEnvelope.amplitude -= globalEnvelope.releaseRate;
                        if (globalEnvelope.amplitude <= 0.0f) {
                            globalEnvelope.amplitude = 0.0f;
                            globalEnvelope.phase = ENVELOPE_IDLE;
                        }
                        break;

                    case ENVELOPE_IDLE:
                    default:
                        globalEnvelope.amplitude = 0.0f;
                        break;
                }

                // Modulate interleaved stereo stream
                audioBuffer[i * 2]     = (int16_t)(audioBuffer[i * 2] * globalEnvelope.amplitude);
                audioBuffer[i * 2 + 1] = (int16_t)(audioBuffer[i * 2 + 1] * globalEnvelope.amplitude);
            }
        }

        portENTER_CRITICAL(&visualAudioMux);
        for (int i = 0; i < BUFFER_SIZE; i++) {
            visualAudio[i] = audioBuffer[i * 2];
        }
        portEXIT_CRITICAL(&visualAudioMux);

        size_t bytesWritten;
        i2s_write(I2S_NUM, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(115200);

    init_gpio();

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);

    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    display.setRotation(2);

    cap.begin(0x5A, &Wire, 1, 1);

    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, TFT1_CS_PIN);

    init_st7789();
    init_gc9a01();

    init_i2s_dac();
    init_mems_mic();

    flute_init();
    wavetable_init();
    sampler_init();

    updateEnvelopeRates();

    xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}