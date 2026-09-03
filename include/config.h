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
#define I2S_MIC_SCK     27
#define I2S_MIC_WS      4
#define I2S_MIC_SD      35

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
#define TFT_DC_PIN      16
#define TFT_RST_PIN     2
#define TFT2_RST_PIN    33

// ============================================================================
// PERIPHERALS & INTERRUPTS
// ============================================================================
#define LED_DATA_PIN    12
#define MPR121_IRQ_PIN  5

// Rotary encoder
#define ENCODER_SW_PIN  34
#define ENCODER_A_PIN   39
#define ENCODER_B_PIN   36

// Two-button resistor ladder
#define BUTTON_ADC_PIN         32
#define BUTTON_PULLUP_OHMS     10000
#define BUTTON1_RESISTOR_OHMS  10000
#define BUTTON2_RESISTOR_OHMS  20000

#define BUTTON_BOTH_MAX_MV  1485
#define BUTTON1_MAX_MV      1925
#define BUTTON2_MAX_MV      2750
#define BUTTON_DEBOUNCE_MS  30

// ============================================================================
// I2C PERIPHERALS
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
#define TOUCH_DELTA_THRESHOLD 6
#define LONG_PRESS_PAD        11
#define LONG_PRESS_MS         1000

// ============================================================================
// AUDIO & SYNTH PARAMETERS
// ============================================================================
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     128
#define MASTER_VOLUME   12000
#define MAX_VOICES      4

// ============================================================================
// MODULATION PARAMETERS
// ============================================================================
#define LFO_WAVE_TYPE           0
#define LFO_RATE_HZ             5.0f
#define VIBRATO_DEPTH_SEMITONES 0.2f
#define TREMOLO_DEPTH           0.3f

enum SynthMode {
    MODE_FLUTE = 0,
    MODE_WAVETABLE = 1,
    MODE_SAMPLER = 2
};

enum EnvelopePhase {
    ENVELOPE_ATTACK = 0,
    ENVELOPE_DECAY,
    ENVELOPE_SUSTAIN,
    ENVELOPE_RELEASE,
    ENVELOPE_IDLE
};

// Global Envelope Structure
struct Envelope {
    bool enabled = false;
    float attackTimeSeconds = 0.8f;
    float decayTimeSeconds = 0.3f;
    float sustainLevel = 0.7f;
    float releaseTimeSeconds = 1.5f;

    float attackRate = 0.0f;
    float decayRate = 0.0f;
    float releaseRate = 0.0f;

    float amplitude = 0.0f;
    EnvelopePhase phase = ENVELOPE_IDLE;
    bool lastGateState = false;
};

extern Envelope globalEnvelope;

// Helper Functions
void toggleEnvelope();
void updateEnvelopeRates();
void setEnvelopeParameters(float a, float d, float s, float r);

struct Voice {
    int padIndex = -1;
    float baseFreq = 0.0f;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;
    float vibratoPhase = 0.0f;
    float amplitude = 0.0f;
    float targetAmplitude = 0.0f;
};

const float noteFreqs[12] = {
    261.63, 277.18, 293.66, 311.13,
    329.63, 349.23, 369.99, 392.00,
    415.30, 440.00, 466.16, 493.88
};

#endif