#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// I2S AUDIO OUTPUT PINS (I2S_NUM_0 - MAX98357A DAC)
// ============================================================================
#define I2S_NUM         I2S_NUM_0
#define I2S_BCK_PIN     26
#define I2S_LRC_PIN     25
#define I2S_DOUT_PIN    17

// ============================================================================
// I2S MEMS MICROPHONE PINS (I2S_NUM_1)
// ============================================================================
#define I2S_MIC_NUM     I2S_NUM_1
#define I2S_MIC_SCK     27  // Serial Clock Out
#define I2S_MIC_WS      4   // Word Select Out (keep dedicated to mic I2S)
#define I2S_MIC_SD      35  // Serial Data In (Dedicated GPI Pin)

// ============================================================================
// SHARED HARDWARE SPI BUS
// ============================================================================
#define SPI_SCLK_PIN    18
#define SPI_MISO_PIN    19
#define SPI_MOSI_PIN    23

// SPI Chip Selects
#define TFT1_CS_PIN     14  // GC9A01 Round TFT
#define TFT2_CS_PIN     15  // ST7789 Secondary Display
#define SD_CS_PIN       13  // SD Card Reader

// Shared Display Control Lines
#define TFT_DC_PIN      16  // Shared Data/Command
#define TFT_RST_PIN     2   // Shared Hardware Reset
#define TFT2_RST_PIN    33  // ST7735 reset; separate from GC9A01 reset

// ============================================================================
// PERIPHERALS & INTERRUPTS
// ============================================================================
#define LED_DATA_PIN    4   // WS28xx LED Strip
#define MPR121_IRQ_PIN  5   // Touch Controller Active-Low IRQ

// ============================================================================
// I2C PERIPHERALS (SSD1306 OLED Screen #3 & MPR121)
// ============================================================================
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS    0x3C

// ============================================================================
// TOUCH CONFIGURATION
// ============================================================================
#define TOUCH_DELTA_THRESHOLD 6     // Drop in filtered capacitance required to trigger a touch
#define LONG_PRESS_PAD        11
#define LONG_PRESS_MS         1000

// ============================================================================
// AUDIO & SYNTH PARAMETERS
// ============================================================================
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     128
#define MASTER_VOLUME   6000
#define MAX_VOICES      4

// ============================================================================
// MODULATION PARAMETERS
// ============================================================================
#define LFO_WAVE_TYPE           0     // 0 = Sine, 1 = Triangle, 2 = Square
#define LFO_RATE_HZ             5.0f  // Speed of modulation (0.1 Hz to 20.0 Hz)

// Modulation Depths
#define VIBRATO_DEPTH_SEMITONES 0.2f  // Pitch movement (+/- fraction of a semitone)
#define TREMOLO_DEPTH           0.3f  // Volume movement (0.0 = off, 1.0 = full attenuation)

enum SynthMode {
    MODE_FLUTE = 0,
    MODE_WAVETABLE = 1,
    MODE_SAMPLER = 2
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

const float noteFreqs[12] = {
    261.63, 277.18, 293.66, 311.13,
    329.63, 349.23, 369.99, 392.00,
    415.30, 440.00, 466.16, 493.88
};

#endif