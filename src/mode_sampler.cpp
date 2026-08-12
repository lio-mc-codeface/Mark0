#include "mode_sampler.h"
#include <driver/i2s.h>
#include <math.h>

static int16_t *sampleBuffer = NULL;
static uint32_t currentSampleLength = 0;
static bool isRecording = false;
static uint32_t recordIndex = 0;

struct SamplerVoice {
    int padIndex = -1;
    float samplePosition = 0.0f;
    float playbackSpeed = 1.0f;
    bool active = false;
};

static SamplerVoice samplerVoices[MAX_VOICES];

// Generate a smooth 440Hz Sine Wave tone as the default boot sample
void generate_default_tone() {
    if (!sampleBuffer) return;

    uint32_t length = 22050; // ~0.50 seconds at 44.1kHz
    if (length > SAMPLE_BUFFER_SIZE) length = SAMPLE_BUFFER_SIZE;

    float freq = 440.0f; // Soft A4 note

    for (uint32_t i = 0; i < length; i++) {
        float progress = (float)i / (float)length;
        float phase = (2.0f * M_PI * freq * i) / SAMPLE_RATE;
        
        // Gentle decay to avoid harsh looping clicks
        float amp = expf(-progress * 3.0f);

        float sampleVal = sinf(phase) * amp;
        sampleBuffer[i] = (int16_t)(sampleVal * 15000.0f);
    }

    currentSampleLength = length;
}

void sampler_init() {
    if (sampleBuffer == NULL) {
        sampleBuffer = (int16_t*) malloc(SAMPLE_BUFFER_SIZE * sizeof(int16_t));
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        samplerVoices[i].padIndex = -1;
        samplerVoices[i].active = false;
    }

    // Load smooth tone on startup
    generate_default_tone();

    // Setup I2S MEMS Mic Driver (I2S_NUM_1)
    i2s_config_t i2s_mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false
    };

    i2s_pin_config_t mic_pin_config = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD
    };

    i2s_driver_install(I2S_NUM_1, &i2s_mic_config, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &mic_pin_config);
}

void sampler_ui_render(Adafruit_SSD1306 &display) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    if (isRecording) {
        display.print("SAMPLER [RECORDING...]");
    } else {
        display.print("MODE: SAMPLER");
    }
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // Waveform display
    if (sampleBuffer && currentSampleLength > 0) {
        uint32_t step = currentSampleLength / 128;
        if (step < 1) step = 1;

        int prevY = 36;
        for (int x = 0; x < 128; x++) {
            uint32_t idx = x * step;
            if (idx >= currentSampleLength) break;

            int y = 36 - (sampleBuffer[idx] / 1200);
            y = constrain(y, 14, 54);

            if (x > 0) {
                display.drawLine(x - 1, prevY, x, y, SSD1306_WHITE);
            }
            prevY = y;
        }
    }

    display.setCursor(0, 56);
    display.print("Pad 0:Rec | 1-11:Play");
    display.display();
}

void sampler_audio_process(int16_t *buffer, uint16_t touchMask) {
    if (!sampleBuffer) return;

    bool recPressed = (touchMask & (1 << 0)) != 0;

    // 1. RECORDING LOGIC (Pad 0)
    if (recPressed) {
        if (!isRecording) {
            isRecording = true;
            recordIndex = 0;
            // Clear active playing voices
            for (int v = 0; v < MAX_VOICES; v++) samplerVoices[v].active = false;
        }

        int32_t rawMicData[BUFFER_SIZE];
        size_t bytesRead = 0;

        i2s_read(I2S_NUM_1, rawMicData, sizeof(rawMicData), &bytesRead, 0);

        uint32_t samplesRead = bytesRead / sizeof(int32_t);
        static float dcOffset = 0.0f;

        for (uint32_t i = 0; i < samplesRead; i++) {
            if (recordIndex < SAMPLE_BUFFER_SIZE) {
                // Convert 24-bit in 32-bit frame down to 16-bit PCM safely
                float pcmSample = (float)(rawMicData[i] >> 14);

                // Simple high-pass filter to strip DC bias offset from MEMS mic
                dcOffset = dcOffset * 0.95f + pcmSample * 0.05f;
                float filtered = (pcmSample - dcOffset) * 2.5f; // Gain boost

                sampleBuffer[recordIndex++] = (int16_t)constrain(filtered, -32767.0f, 32767.0f);
            }
        }

        // Silence output speakers while recording
        memset(buffer, 0, BUFFER_SIZE * 2 * sizeof(int16_t));
        return;
    } else if (isRecording) {
        // Stop recording when Pad 0 is released
        isRecording = false;
        if (recordIndex > 100) { // Keep sample if held longer than a click
            currentSampleLength = recordIndex;
        }
    }

    // 2. PLAYBACK LOGIC (Pads 1 - 11)
    float rootFreq = noteFreqs[1]; // Pad 1 = 1.0x root pitch

    for (uint8_t pad = 1; pad < 12; pad++) {
        if (touchMask & (1 << pad)) {
            bool playing = false;
            for (int v = 0; v < MAX_VOICES; v++) {
                if (samplerVoices[v].padIndex == pad && samplerVoices[v].active) {
                    playing = true;
                    break;
                }
            }
            if (!playing) {
                for (int v = 0; v < MAX_VOICES; v++) {
                    if (!samplerVoices[v].active) {
                        samplerVoices[v].padIndex = pad;
                        samplerVoices[v].samplePosition = 0.0f;
                        samplerVoices[v].playbackSpeed = noteFreqs[pad] / rootFreq;
                        samplerVoices[v].active = true;
                        break;
                    }
                }
            }
        }
    }

    // Release voices on pad touch release
    for (int v = 0; v < MAX_VOICES; v++) {
        if (samplerVoices[v].active) {
            uint8_t pad = samplerVoices[v].padIndex;
            if (pad < 12 && !(touchMask & (1 << pad))) {
                samplerVoices[v].active = false;
                samplerVoices[v].padIndex = -1;
            }
        }
    }

    // 3. MIXING LOOP
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float mixedSample = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            if (samplerVoices[v].active) {
                uint32_t idx0 = (uint32_t)samplerVoices[v].samplePosition;
                uint32_t idx1 = idx0 + 1;

                if (idx0 < currentSampleLength) {
                    float frac = samplerVoices[v].samplePosition - idx0;
                    float s0 = sampleBuffer[idx0];
                    float s1 = (idx1 < currentSampleLength) ? sampleBuffer[idx1] : 0.0f;

                    float interp = s0 + frac * (s1 - s0);
                    mixedSample += interp;

                    samplerVoices[v].samplePosition += samplerVoices[v].playbackSpeed;
                } else {
                    samplerVoices[v].active = false;
                }
            }
        }

        int16_t sample = (int16_t)(mixedSample * (MASTER_VOLUME / MAX_VOICES) / 8000.0f);
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}