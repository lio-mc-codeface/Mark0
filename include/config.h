#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// I2S Hardware Pins
#define I2S_NUM         I2S_NUM_0
#define I2S_BCK_PIN     26
#define I2S_LRC_PIN     25
#define I2S_DOUT_PIN    17

// OLED Display Configuration
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS    0x3C

// Touch Configuration
#define TOUCH_THRESHOLD 12
#define LONG_PRESS_PAD  11
#define LONG_PRESS_MS   1000

// Audio Parameters
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     128
#define MASTER_VOLUME   6000
#define MAX_VOICES      4

// ============================================================================
// MODULATION PARAMETERS
// ============================================================================
#define LFO_WAVE_TYPE       0       // 0 = Sine, 1 = Triangle, 2 = Square
#define LFO_RATE_HZ         5.0f    // Speed of modulation (0.1 Hz to 20.0 Hz)

// Modulation Depths
#define VIBRATO_DEPTH_SEMITONES 0.2f  // Pitch movement (+/- fraction of a semitone)
#define TREMOLO_DEPTH           0.3f  // Volume movement (0.0 = off, 1.0 = full attenuation)

enum SynthMode {
    MODE_FLUTE = 0,
    MODE_WAVETABLE = 1
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