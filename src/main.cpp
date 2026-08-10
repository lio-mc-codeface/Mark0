#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/i2s.h"

#include "config.h"
#include "mode_flute.h"
#include "mode_wavetable.h"

volatile SynthMode currentMode = MODE_FLUTE;
volatile uint16_t activeTouchMask = 0;

Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

        // Long Press Mode Toggle
        if (mask & (1 << LONG_PRESS_PAD)) {
            if (pad11PressStart == 0) pad11PressStart = millis();
            else if (!pad11Handled && (millis() - pad11PressStart >= LONG_PRESS_MS)) {
                currentMode = (currentMode == MODE_FLUTE) ? MODE_WAVETABLE : MODE_FLUTE;
                pad11Handled = true;
            }
        } else {
            pad11PressStart = 0;
            pad11Handled = false;
        }

        activeTouchMask = mask;

        // Dispatch UI rendering to active mode
        if (currentMode == MODE_FLUTE) {
            flute_ui_render(display, activeTouchMask);
        } else {
            wavetable_ui_render(display);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void audioTask(void *pvParameters) {
    size_t bytes_written;

    while (1) {
        // Dispatch Audio generation to active mode
        if (currentMode == MODE_FLUTE) {
            flute_audio_process(audio_buffer, activeTouchMask);
        } else {
            wavetable_audio_process(audio_buffer, activeTouchMask);
        }

        i2s_write(I2S_NUM, audio_buffer, sizeof(audio_buffer), &bytes_written, portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    Wire.setClock(400000);

    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    display.setRotation(2);

    cap.begin(0x5A, &Wire);

    init_i2s();
    flute_init();
    wavetable_init();

    xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}