#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GC9A01A.h>
#include "driver/i2s.h"

#include "config.h"
#include "mode_flute.h"
#include "mode_wavetable.h"
#include "mode_sampler.h"

// ============================================================================
// GC9A01 SPI DISPLAY PINS (From System Pinout Table)
// ============================================================================
#define TFT_CS    14
#define TFT_DC    16
#define TFT_RST    2
#define TFT_BL    15

// Global Objects
volatile SynthMode currentMode = MODE_FLUTE;
volatile uint16_t activeTouchMask = 0;

Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

int16_t audio_buffer[BUFFER_SIZE * 2];

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

void init_gc9a01() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH); // Turn on backlight

    // 1. Force Hard-Reset sequence to wake up display driver
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, HIGH);
    delay(10);
    digitalWrite(TFT_RST, LOW);
    delay(50);
    digitalWrite(TFT_RST, HIGH);
    delay(120);

    // 2. Explicitly bind Hardware SPI pins (SCLK=13, MISO=-1, MOSI=12, CS=14)
    SPI.begin(13, -1, 12, 14);

    // 3. Begin TFT initialization
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(GC9A01A_BLACK);

    // Initial Hello World frame
    tft.drawCircle(120, 120, 115, GC9A01A_CYAN);
    tft.setTextColor(GC9A01A_WHITE);
    tft.setTextSize(2);
    tft.setCursor(55, 110);
    tft.print("Hello World");
}

void uiTask(void *pvParameters) {
    unsigned long pad11PressStart = 0;
    bool pad11Handled = false;

    while (1) {
        uint16_t mask = 0;
        for (uint8_t i = 0; i < 12; i++) {
            uint16_t val = cap.filteredData(i);
            if (val < TOUCH_THRESHOLD && val > 0) {
                mask |= (1 << i);
            }
        }

        // Long Press Mode Toggle: FLUTE -> WAVETABLE -> SAMPLER -> FLUTE
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

        // Dispatch UI rendering to active mode for SSD1306 OLED
        if (currentMode == MODE_FLUTE) {
            flute_ui_render(display, activeTouchMask);
        } else if (currentMode == MODE_WAVETABLE) {
            wavetable_ui_render(display);
        } else if (currentMode == MODE_SAMPLER) {
            sampler_ui_render(display);
        }

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

        size_t bytesWritten;
        i2s_write(I2S_NUM, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize I2C Bus for SSD1306 & MPR121
    Wire.begin(21, 22);
    Wire.setClock(400000);

    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    display.setRotation(2);

    cap.begin(0x5A, &Wire);

    // Initialize GC9A01 SPI Display
    init_gc9a01();

    // Initialize Audio & Synthesizer Engines
    init_i2s();
    flute_init();
    wavetable_init();
    sampler_init();

    // Launch FreeRTOS Tasks
    xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}