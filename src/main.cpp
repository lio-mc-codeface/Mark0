#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_ST7735.h>
#include "driver/i2s.h"

#include "config.h"
#include "mode_flute.h"
#include "mode_wavetable.h"
#include "mode_sampler.h"

// Global Objects
volatile SynthMode currentMode = MODE_FLUTE;
volatile uint16_t activeTouchMask = 0;
uint16_t restingCapacitance[12] = {0};

Adafruit_MPR121 cap = Adafruit_MPR121();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_GC9A01A tft(TFT1_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
Adafruit_ST7735 tft2(TFT2_CS_PIN, TFT_DC_PIN, TFT2_RST_PIN);

void init_gpio() {
    // Peripherals
    pinMode(MPR121_IRQ_PIN, INPUT_PULLUP);

    // Deselect non-active SPI devices immediately
    pinMode(TFT2_CS_PIN, OUTPUT);
    digitalWrite(TFT2_CS_PIN, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
}

// MAX98357A I2S DAC Output Initialization (I2S_NUM_0)
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

// INMP441 / MEMS Mic I2S Input Initialization (I2S_NUM_1)
void init_mems_mic() {
    i2s_config_t mic_i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,                          // 44100 Hz
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,       // MEMS 24-bit in 32-bit container
        // INMP441 L/R tied to 3V3 selects the right I2S slot.
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .use_apll = true,                                    // <--- SET TO TRUE for I2S_NUM_1
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t mic_pin_config = {
        .bck_io_num = I2S_MIC_SCK, // GPIO 27
        .ws_io_num = I2S_MIC_WS,   // GPIO 4
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD  // GPIO 35
    };

    // Uninstall first just in case a previous attempt locked the peripheral
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
    // Hardware Reset sequence using shared reset pin
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(10);
    digitalWrite(TFT_RST_PIN, LOW);
    delay(50);
    digitalWrite(TFT_RST_PIN, HIGH);
    delay(120);

    // Begin TFT initialization
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

void init_st7735() {
    // TFT2 has its own reset line; CS was held HIGH in init_gpio().
    // The 0.96-inch panel is the common 128x160 ST7735 variant.
    tft2.initR(INITR_BLACKTAB);
    tft2.setRotation(1);
    tft2.invertDisplay(false);
    tft2.fillScreen(ST77XX_BLACK);
    tft2.setTextColor(ST77XX_WHITE);
    tft2.setTextSize(2);
    tft2.setCursor(28, 56);
    tft2.print("Hello World");
}

void uiTask(void *pvParameters) {
    unsigned long pad11PressStart = 0;
    bool pad11Handled = false;

    // --- Calibrate Resting Capacitance Baseline ---
    vTaskDelay(pdMS_TO_TICKS(100));
    for (uint8_t i = 0; i < 12; i++) {
        restingCapacitance[i] = cap.filteredData(i);
    }

    while (1) {
        uint16_t mask = 0;
        
        // Dynamic Delta Touch Detection
        for (uint8_t i = 0; i < 12; i++) {
            uint16_t val = cap.filteredData(i);
            
            // Check if filtered reading dropped significantly below resting value
            if (val > 0 && restingCapacitance[i] > val) {
                uint16_t delta = restingCapacitance[i] - val;
                if (delta >= TOUCH_DELTA_THRESHOLD) {
                    mask |= (1 << i);
                }
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
            // The sampler owns microphone reads so recording and playback share
            // one I2S consumer.
            sampler_audio_process(audioBuffer, activeTouchMask);
        }

        size_t bytesWritten;
        i2s_write(I2S_NUM, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize GPIO Inputs/Outputs
    init_gpio();

    // Initialize I2C Bus for SSD1306 & MPR121
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000); // 100kHz stability

    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    display.setRotation(2);

    cap.begin(0x5A, &Wire, 1, 1);

    // Bind the shared hardware SPI bus before initializing either display.
    SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, TFT1_CS_PIN);

    // Initialize both displays independently on the shared SPI bus.
    init_st7735();
    init_gc9a01();

    // Initialize Both I2S Engines (DAC output + Mic input)
    init_i2s_dac();
    init_mems_mic();

    // Initialize Synthesizer Engines
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