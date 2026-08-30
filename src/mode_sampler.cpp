#include "mode_sampler.h"
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <math.h>

static const char *SAMPLE_PATH = "/sample.wav";
static const uint32_t WAV_HEADER_SIZE = 44;
static const uint32_t SAMPLE_CACHE_SIZE = 256;
static int16_t *sampleBuffer = NULL;
static uint32_t currentSampleLength = 0;
static bool isRecording = false;
static bool sdReady = false;
static bool sdSampleLoaded = false;
static File sampleFile;
static uint32_t recordedBytes = 0;

struct SamplerVoice {
    int padIndex = -1;
    float samplePosition = 0.0f;
    float playbackSpeed = 1.0f;
    bool active = false;
    float amplitude = 0.0f;          // soft attack/release
    float targetAmplitude = 0.0f;    // soft attack/release
    int32_t cacheBlock = -1;
    uint16_t cachedSamples = 0;
    int16_t cache[SAMPLE_CACHE_SIZE];
};

static SamplerVoice samplerVoices[MAX_VOICES];

void generate_default_tone() {
    if (!sampleBuffer) return;
    uint32_t length = 22050;
    for (uint32_t i = 0; i < length; i++) {
        float progress = (float)i / (float)length;
        float phase = (2.0f * M_PI * 440.0f * i) / SAMPLE_RATE;
        sampleBuffer[i] = (int16_t)(sinf(phase) * expf(-progress * 3.0f) * 15000.0f);
    }
    currentSampleLength = length;
}

static void write_wav_header(File &file, uint32_t dataLength) {
    uint8_t header[WAV_HEADER_SIZE] = {
        'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 16, 0,
        'd', 'a', 't', 'a', 0, 0, 0, 0
    };
    uint32_t riffLength = 36 + dataLength;
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate = sampleRate * sizeof(int16_t);
    memcpy(header + 4, &riffLength, sizeof(riffLength));
    memcpy(header + 24, &sampleRate, sizeof(sampleRate));
    memcpy(header + 28, &byteRate, sizeof(byteRate));
    memcpy(header + 40, &dataLength, sizeof(dataLength));
    file.seek(0);
    file.write(header, sizeof(header));
}

static bool load_sample_block(SamplerVoice &voice, uint32_t sampleIndex) {
    if (!sdReady || !sampleFile || sampleIndex >= currentSampleLength) return false;
    uint32_t block = sampleIndex / SAMPLE_CACHE_SIZE;
    if (voice.cacheBlock == (int32_t)block) return true;
    uint32_t offset = WAV_HEADER_SIZE + block * SAMPLE_CACHE_SIZE * sizeof(int16_t);
    if (!sampleFile.seek(offset)) return false;
    size_t bytesRead = sampleFile.read((uint8_t *)voice.cache,
                                       SAMPLE_CACHE_SIZE * sizeof(int16_t));
    voice.cachedSamples = bytesRead / sizeof(int16_t);
    voice.cacheBlock = block;
    return voice.cachedSamples > 0;
}

static int16_t read_sample(SamplerVoice &voice, uint32_t sampleIndex) {
    if (!load_sample_block(voice, sampleIndex)) return 0;
    uint32_t offset = sampleIndex % SAMPLE_CACHE_SIZE;
    return offset < voice.cachedSamples ? voice.cache[offset] : 0;
}

void sampler_init() {
    sampleBuffer = (int16_t *)malloc(32768 * sizeof(int16_t));
    sdReady = SD.begin(SD_CS_PIN, SPI, 10000000);
    if (sdReady) {
        sampleFile = SD.open(SAMPLE_PATH, FILE_READ);
        if (sampleFile && sampleFile.size() > WAV_HEADER_SIZE) {
            currentSampleLength = (sampleFile.size() - WAV_HEADER_SIZE) / sizeof(int16_t);
            sdSampleLoaded = true;
            Serial.printf("SD sample loaded: %lu samples\n", currentSampleLength);
        } else {
            Serial.println("No saved SD sample; using default tone");
        }
    } else {
        Serial.println("SD unavailable; sampler recording disabled");
    }
    for (int i = 0; i < MAX_VOICES; i++) {
        samplerVoices[i].padIndex = -1;
        samplerVoices[i].active = false;
        samplerVoices[i].amplitude = 0.0f;
        samplerVoices[i].targetAmplitude = 0.0f;
        samplerVoices[i].cacheBlock = -1;
    }
    if (currentSampleLength == 0) generate_default_tone();
}

void sampler_ui_render(Adafruit_SSD1306 &display) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(isRecording ? "SAMPLER [RECORDING...]" : "MODE: SAMPLER");
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    if (sampleBuffer && currentSampleLength > 0 && currentSampleLength <= 32768) {
        uint32_t step = currentSampleLength / 128;
        if (step < 1) step = 1;
        int prevY = 36;
        for (int x = 0; x < 128; x++) {
            uint32_t index = x * step;
            if (index >= currentSampleLength) break;
            int y = constrain(36 - (sampleBuffer[index] / 1200), 14, 54);
            if (x > 0) display.drawLine(x - 1, prevY, x, y, SSD1306_WHITE);
            prevY = y;
        }
    }
    display.setCursor(0, 56);
    display.print("Pad 0:Rec | 1-11:Play");
    display.display();
}

void sampler_audio_process(int16_t *buffer, uint16_t touchMask) {
    bool recPressed = (touchMask & (1 << 0)) != 0;

    // ========== RECORDING ==========
    if (recPressed) {
        if (!isRecording) {
            if (!sdReady) {
                memset(buffer, 0, BUFFER_SIZE * 2 * sizeof(int16_t));
                return;
            }
            if (sampleFile) sampleFile.close();
            SD.remove(SAMPLE_PATH);
            sampleFile = SD.open(SAMPLE_PATH, FILE_WRITE);
            if (!sampleFile) {
                Serial.println("Could not open SD sample for recording");
                memset(buffer, 0, BUFFER_SIZE * 2 * sizeof(int16_t));
                return;
            }
            uint8_t emptyHeader[WAV_HEADER_SIZE] = {};
            sampleFile.write(emptyHeader, sizeof(emptyHeader));
            recordedBytes = 0;
            isRecording = true;
            for (int v = 0; v < MAX_VOICES; v++) {
                samplerVoices[v].active = false;
                samplerVoices[v].amplitude = 0.0f;
                samplerVoices[v].targetAmplitude = 0.0f;
            }
        }

        int32_t rawMicData[BUFFER_SIZE];
        int16_t pcmBlock[BUFFER_SIZE];
        size_t bytesRead = 0;
        i2s_read(I2S_MIC_NUM, rawMicData, sizeof(rawMicData), &bytesRead, 0);
        uint32_t samplesRead = bytesRead / sizeof(int32_t);

        static float dcOffset = 0.0f;
        for (uint32_t i = 0; i < samplesRead; i++) {
            float pcmSample = (float)(rawMicData[i] >> 14);
            dcOffset = dcOffset * 0.95f + pcmSample * 0.05f;
            pcmBlock[i] = (int16_t)constrain((pcmSample - dcOffset) * 2.5f, -32767.0f, 32767.0f);
        }

        if (samplesRead > 0) {
            sampleFile.write((uint8_t *)pcmBlock, samplesRead * sizeof(int16_t));
            recordedBytes += samplesRead * sizeof(int16_t);
        }

        memset(buffer, 0, BUFFER_SIZE * 2 * sizeof(int16_t));
        return;
    }

    // Stop recording
    if (isRecording) {
        isRecording = false;
        if (sampleFile) {
            write_wav_header(sampleFile, recordedBytes);
            sampleFile.flush();
            sampleFile.close();
        }
        if (recordedBytes > 200) {
            currentSampleLength = recordedBytes / sizeof(int16_t);
            sampleFile = SD.open(SAMPLE_PATH, FILE_READ);
            sdSampleLoaded = sampleFile ? true : false;
            Serial.printf("SD sample recorded: %lu samples\n", currentSampleLength);
        }
    }

    // ========== PLAYBACK ==========
    float rootFreq = noteFreqs[1];

    // Voice allocation
    for (uint8_t pad = 1; pad < 12; pad++) {
        if (!(touchMask & (1 << pad))) continue;

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
                    samplerVoices[v].amplitude = 0.0f;
                    samplerVoices[v].targetAmplitude = 1.0f;   // soft attack
                    samplerVoices[v].cacheBlock = -1;
                    break;
                }
            }
        }
    }

    // Voice release
    for (int v = 0; v < MAX_VOICES; v++) {
        if (samplerVoices[v].active) {
            uint8_t pad = samplerVoices[v].padIndex;
            if (pad < 12 && !(touchMask & (1 << pad))) {
                samplerVoices[v].targetAmplitude = 0.0f;   // soft release
            }
        }
    }

    // DSP loop
    for (int i = 0; i < BUFFER_SIZE; i++) {
        float mixedSample = 0.0f;
        int activeVoiceCount = 0;

        for (int v = 0; v < MAX_VOICES; v++) {
            if (!samplerVoices[v].active) continue;

            // Soft attack / release
            const float ampSpeed = 0.0025f;

            if (samplerVoices[v].amplitude < samplerVoices[v].targetAmplitude) {
                samplerVoices[v].amplitude += ampSpeed;
                if (samplerVoices[v].amplitude > samplerVoices[v].targetAmplitude)
                    samplerVoices[v].amplitude = samplerVoices[v].targetAmplitude;
            } else if (samplerVoices[v].amplitude > samplerVoices[v].targetAmplitude) {
                samplerVoices[v].amplitude -= ampSpeed;
                if (samplerVoices[v].amplitude < samplerVoices[v].targetAmplitude)
                    samplerVoices[v].amplitude = samplerVoices[v].targetAmplitude;
            }

            // Free voice when fully faded out
            if (samplerVoices[v].targetAmplitude <= 0.0f && samplerVoices[v].amplitude < 0.001f) {
                samplerVoices[v].active = false;
                samplerVoices[v].padIndex = -1;
                continue;
            }

            if (samplerVoices[v].amplitude < 0.001f) continue;

            activeVoiceCount++;

            uint32_t index0 = (uint32_t)samplerVoices[v].samplePosition;
            uint32_t index1 = index0 + 1;

            if (index0 >= currentSampleLength) {
                samplerVoices[v].active = false;
                continue;
            }

            float fraction = samplerVoices[v].samplePosition - index0;
            float sample0 = sdSampleLoaded ? read_sample(samplerVoices[v], index0) : sampleBuffer[index0];
            float sample1 = (index1 < currentSampleLength)
                                ? (sdSampleLoaded ? read_sample(samplerVoices[v], index1) : sampleBuffer[index1])
                                : 0.0f;

            float sample = sample0 + fraction * (sample1 - sample0);
            mixedSample += sample * samplerVoices[v].amplitude;

            samplerVoices[v].samplePosition += samplerVoices[v].playbackSpeed;
        }

        float voiceScale = (activeVoiceCount > 0) ? (1.0f / activeVoiceCount) : 1.0f;
        float gain = (float)MASTER_VOLUME * voiceScale / 8000.0f * 0.85f;

        int16_t sample = (int16_t)constrain(mixedSample * gain, -32767.0f, 32767.0f);
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}