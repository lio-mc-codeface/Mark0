#include "mode_wavetable.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <math.h>

// Global modulation variables initialized from config defaults
float g_lfoRateHz    = LFO_RATE_HZ;
float g_vibratoDepth = VIBRATO_DEPTH_SEMITONES;
float g_tremoloDepth = TREMOLO_DEPTH;

static float customWavetable[128];
static Voice waveVoices[MAX_VOICES];

// Web Server and WebSockets
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// HTML + JavaScript Interface embedded in flash memory
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Tone-ner Waveform Drawer</title>
    <style>
        body {
            background-color: #121212;
            color: #ffffff;
            font-family: sans-serif;
            text-align: center;
            margin: 0;
            padding: 10px;
            touch-action: none;
        }
        .header {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
            margin-bottom: 8px;
        }
        h2 { margin: 0; }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background-color: #ff3333;
            display: inline-block;
        }
        .status-dot.online { background-color: #00ff66; }
        canvas {
            background-color: #1e1e1e;
            border: 2px solid #00e5ff;
            border-radius: 8px;
            width: 90vw;
            height: 45vw;
            max-width: 500px;
            max-height: 250px;
            cursor: crosshair;
        }
        .controls {
            max-width: 500px;
            margin: 10px auto;
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .slider-container {
            display: flex;
            align-items: center;
            justify-content: space-between;
            background: #252525;
            padding: 8px 12px;
            border-radius: 8px;
        }
        .slider-container label { font-size: 13px; font-weight: bold; width: 110px; text-align: left; }
        input[type=range] { flex-grow: 1; margin: 0 10px; }
        .val-disp { font-size: 13px; width: 35px; text-align: right; font-family: monospace; }
        .btn-container { display: flex; justify-content: center; gap: 8px; flex-wrap: wrap; margin-top: 5px; }
        button {
            background: #00e5ff;
            color: #000;
            border: none;
            padding: 8px 14px;
            font-weight: bold;
            border-radius: 5px;
            font-size: 14px;
        }
        button.btn-sync { background: #ffaa00; }
    </style>
</head>
<body>
    <div class="header">
        <h2>Tone-ner Wave Drawer</h2>
        <span id="statusDot" class="status-dot"></span>
    </div>

    <canvas id="waveCanvas" width="128" height="128"></canvas>

    <div class="controls">
        <!-- Waveform Smoothing -->
        <div class="slider-container">
            <label for="smoothSlider">Smoothing:</label>
            <input type="range" id="smoothSlider" min="0" max="20" value="0" oninput="applySmoothing()">
            <span id="smoothVal" class="val-disp">0</span>
        </div>

        <!-- LFO Rate -->
        <div class="slider-container">
            <label for="lfoRateSlider">LFO Speed:</label>
            <input type="range" id="lfoRateSlider" min="0.1" max="20.0" step="0.1" value="5.0" oninput="sendModulationParams()">
            <span id="lfoRateVal" class="val-disp">5.0</span>
        </div>

        <!-- Vibrato Depth -->
        <div class="slider-container">
            <label for="vibratoSlider">Vibrato (Pitch):</label>
            <input type="range" id="vibratoSlider" min="0.0" max="2.0" step="0.05" value="0.2" oninput="sendModulationParams()">
            <span id="vibratoVal" class="val-disp">0.2</span>
        </div>

        <!-- Tremolo Depth -->
        <div class="slider-container">
            <label for="tremoloSlider">Tremolo (Vol):</label>
            <input type="range" id="tremoloSlider" min="0.0" max="1.0" step="0.05" value="0.3" oninput="sendModulationParams()">
            <span id="tremoloVal" class="val-disp">0.3</span>
        </div>

        <div class="btn-container">
            <button class="btn-sync" onclick="forceSync()">⚡ Sync ESP</button>
            <button onclick="presetSine()">Sine</button>
            <button onclick="presetSquare()">Square</button>
            <button onclick="presetSaw()">Saw</button>
        </div>
    </div>

    <script>
        const canvas = document.getElementById('waveCanvas');
        const ctx = canvas.getContext('2d');
        const statusDot = document.getElementById('statusDot');
        
        const rawWave = new Float32Array(128);
        const processedWave = new Float32Array(128);
        
        let drawing = false;
        let websocket = null;

        function initWebSocket() {
            websocket = new WebSocket(`ws://${window.location.hostname}/ws`);

            websocket.onopen = function() {
                statusDot.classList.add('online');
                sendWaveform();
                sendModulationParams();
            };

            websocket.onclose = function() {
                statusDot.classList.remove('online');
                setTimeout(initWebSocket, 1000);
            };

            websocket.onerror = function(err) {
                statusDot.classList.remove('online');
                websocket.close();
            };
        }

        function drawGrid() {
            ctx.fillStyle = '#1e1e1e';
            ctx.fillRect(0, 0, 128, 128);
            ctx.strokeStyle = '#333333';
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(0, 64); ctx.lineTo(128, 64);
            ctx.stroke();
        }

        function renderWave() {
            drawGrid();
            ctx.strokeStyle = '#00e5ff';
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (let x = 0; x < 128; x++) {
                let y = 64 - (processedWave[x] * 60);
                if (x === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        function sendWaveform() {
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                websocket.send(processedWave.buffer);
            }
        }

        function sendModulationParams() {
            const rate = parseFloat(document.getElementById('lfoRateSlider').value);
            const vibrato = parseFloat(document.getElementById('vibratoSlider').value);
            const tremolo = parseFloat(document.getElementById('tremoloSlider').value);

            document.getElementById('lfoRateVal').innerText = rate.toFixed(1);
            document.getElementById('vibratoVal').innerText = vibrato.toFixed(2);
            document.getElementById('tremoloVal').innerText = tremolo.toFixed(2);

            if (websocket && websocket.readyState === WebSocket.OPEN) {
                const msg = `MOD:${rate}:${vibrato}:${tremolo}`;
                websocket.send(msg);
            }
        }

        function forceSync() {
            if (!websocket || websocket.readyState !== WebSocket.OPEN) {
                initWebSocket();
            } else {
                sendWaveform();
                sendModulationParams();
            }
        }

        function applySmoothing() {
            const passes = parseInt(document.getElementById('smoothSlider').value);
            document.getElementById('smoothVal').innerText = passes;

            processedWave.set(rawWave);

            for (let p = 0; p < passes; p++) {
                let temp = new Float32Array(128);
                for (let i = 0; i < 128; i++) {
                    let prev = processedWave[(i - 1 + 128) % 128];
                    let curr = processedWave[i];
                    let next = processedWave[(i + 1) % 128];
                    temp[i] = (prev + curr + next) / 3.0;
                }
                processedWave.set(temp);
            }

            renderWave();
            sendWaveform();
        }

        function updatePoint(e) {
            const rect = canvas.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;

            let x = Math.floor(((clientX - rect.left) / rect.width) * 128);
            let y = (clientY - rect.top) / rect.height;

            x = Math.max(0, Math.min(127, x));
            let val = 1.0 - (y * 2.0);
            val = Math.max(-1.0, Math.min(1.0, val));

            rawWave[x] = val;
            applySmoothing();
        }

        canvas.addEventListener('mousedown', (e) => { drawing = true; updatePoint(e); });
        canvas.addEventListener('mousemove', (e) => { if (drawing) updatePoint(e); });
        window.addEventListener('mouseup', () => { if (drawing) drawing = false; });

        canvas.addEventListener('touchstart', (e) => { drawing = true; updatePoint(e); e.preventDefault(); });
        canvas.addEventListener('touchmove', (e) => { if (drawing) updatePoint(e); e.preventDefault(); });
        window.addEventListener('touchend', () => { if (drawing) drawing = false; });

        function presetSine() {
            for (let i = 0; i < 128; i++) rawWave[i] = Math.sin((2 * Math.PI * i) / 128);
            applySmoothing();
        }
        function presetSquare() {
            for (let i = 0; i < 128; i++) rawWave[i] = i < 64 ? 0.8 : -0.8;
            applySmoothing();
        }
        function presetSaw() {
            for (let i = 0; i < 128; i++) rawWave[i] = 1.0 - (2.0 * i / 128);
            applySmoothing();
        }

        presetSine();
        initWebSocket();
    </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
        // Option 1: Binary wavetable buffer (512 bytes = 128 floats)
        if (len == 128 * sizeof(float)) {
            memcpy(customWavetable, data, 128 * sizeof(float));
        } 
        // Option 2: Text command for modulation parameters
        else {
            data[len] = '\0'; // Null-terminate text string
            String msg = String((char*)data);
            if (msg.startsWith("MOD:")) {
                int firstColon  = msg.indexOf(':', 4);
                int secondColon = msg.indexOf(':', firstColon + 1);

                if (firstColon > 0 && secondColon > 0) {
                    g_lfoRateHz    = msg.substring(4, firstColon).toFloat();
                    g_vibratoDepth = msg.substring(firstColon + 1, secondColon).toFloat();
                    g_tremoloDepth = msg.substring(secondColon + 1).toFloat();
                }
            }
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

void wavetable_init() {
    for (int i = 0; i < 128; i++) {
        customWavetable[i] = sinf((2.0f * M_PI * i) / 128.0f);
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        waveVoices[i].padIndex = -1;
        waveVoices[i].amplitude = 0.0f;
        waveVoices[i].targetAmplitude = 0.0f;
    }

    WiFi.softAP("Tone-ner-Synth");

    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.begin();
}

void wavetable_ui_render(Adafruit_SSD1306 &display) {
    static int prev_y = 42;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("MODE: WAVETABLE");
    display.setCursor(0, 8);
    display.print("WiFi: Tone-ner-Synth");
    display.drawFastHLine(0, 18, 128, SSD1306_WHITE);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        float sampleVal = customWavetable[x];
        int y = 42 - (int)(sampleVal * 16.0f);
        y = constrain(y, 20, 62);

        if (x > 0) {
            display.drawLine(x - 1, prev_y, x, y, SSD1306_WHITE);
        } else {
            prev_y = y;
        }
    }

    display.display();
}

static float lfoPhase = 0.0f;

void wavetable_audio_process(int16_t *buffer, uint16_t touchMask) {
    ws.cleanupClients();

    // 1. Voice Allocation
    for (uint8_t pad = 0; pad < 12; pad++) {
        if (touchMask & (1 << pad)) {
            bool playing = false;
            for (int v = 0; v < MAX_VOICES; v++) {
                if (waveVoices[v].padIndex == pad) {
                    playing = true;
                    break;
                }
            }
            if (!playing) {
                for (int v = 0; v < MAX_VOICES; v++) {
                    if (waveVoices[v].padIndex == -1) {
                        waveVoices[v].padIndex = pad;
                        waveVoices[v].baseFreq = noteFreqs[pad];
                        waveVoices[v].phase1 = 0.0f;
                        waveVoices[v].amplitude = 0.0f;
                        waveVoices[v].targetAmplitude = 1.0f;   // soft attack
                        break;
                    }
                }
            }
        }
    }

    // 2. Voice Release
    for (int v = 0; v < MAX_VOICES; v++) {
        if (waveVoices[v].padIndex != -1 && !(touchMask & (1 << waveVoices[v].padIndex))) {
            waveVoices[v].targetAmplitude = 0.0f;   // soft release
        }
    }

    // 3. DSP
    float lfoInc = g_lfoRateHz / (float)SAMPLE_RATE;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        float lfoVal = sinf(2.0f * M_PI * lfoPhase);
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        float tremoloGain = 1.0f - (g_tremoloDepth * (0.5f + 0.5f * lfoVal));
        float mixedSample = 0.0f;
        int activeVoiceCount = 0;

        for (int v = 0; v < MAX_VOICES; v++) {
            // Soft attack / release
            const float ampSpeed = 0.0025f;

            if (waveVoices[v].amplitude < waveVoices[v].targetAmplitude) {
                waveVoices[v].amplitude += ampSpeed;
                if (waveVoices[v].amplitude > waveVoices[v].targetAmplitude)
                    waveVoices[v].amplitude = waveVoices[v].targetAmplitude;
            } else if (waveVoices[v].amplitude > waveVoices[v].targetAmplitude) {
                waveVoices[v].amplitude -= ampSpeed;
                if (waveVoices[v].amplitude < waveVoices[v].targetAmplitude)
                    waveVoices[v].amplitude = waveVoices[v].targetAmplitude;
            }

            // Free voice when fully silent
            if (waveVoices[v].padIndex != -1 &&
                waveVoices[v].targetAmplitude <= 0.0f &&
                waveVoices[v].amplitude < 0.001f) {
                waveVoices[v].padIndex = -1;
            }

            if (waveVoices[v].amplitude > 0.001f) {
                activeVoiceCount++;

                float pitchFactor = powf(2.0f, (lfoVal * g_vibratoDepth) / 12.0f);
                float currentFreq = waveVoices[v].baseFreq * pitchFactor;
                float phaseInc = (128.0f * currentFreq) / SAMPLE_RATE;

                int index0 = (int)waveVoices[v].phase1;
                int index1 = (index0 + 1) % 128;
                float frac = waveVoices[v].phase1 - index0;

                float sample = customWavetable[index0] +
                               frac * (customWavetable[index1] - customWavetable[index0]);

                mixedSample += sample * waveVoices[v].amplitude * tremoloGain;

                waveVoices[v].phase1 += phaseInc;
                if (waveVoices[v].phase1 >= 128.0f) waveVoices[v].phase1 -= 128.0f;
            }
        }

// Constant headroom – no volume jump when adding/removing notes
float gainScale = (float)MASTER_VOLUME / (float)MAX_VOICES * 0.85f;

        int16_t sample = (int16_t)constrain(mixedSample * gainScale, -32767.0f, 32767.0f);
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}