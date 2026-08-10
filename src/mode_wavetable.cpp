#include "mode_wavetable.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <math.h>

static float customWavetable[128];
static Voice waveVoices[MAX_VOICES];

// Web Server and WebSockets
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// HTML + JavaScript Interface embedded in flash memory
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
        h2 { margin-bottom: 8px; }
        canvas {
            background-color: #1e1e1e;
            border: 2px solid #00e5ff;
            border-radius: 8px;
            width: 90vw;
            height: 50vw;
            max-width: 500px;
            max-height: 280px;
            cursor: crosshair;
        }
        .controls {
            max-width: 500px;
            margin: 15px auto;
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        .slider-container {
            display: flex;
            align-items: center;
            justify-content: space-between;
            background: #252525;
            padding: 10px 15px;
            border-radius: 8px;
        }
        .slider-container label { font-size: 14px; font-weight: bold; }
        input[type=range] { flex-grow: 1; margin: 0 12px; }
        .btn-container { display: flex; justify-content: center; gap: 8px; }
        button {
            background: #00e5ff;
            color: #000;
            border: none;
            padding: 10px 18px;
            font-weight: bold;
            border-radius: 5px;
            font-size: 15px;
        }
    </style>
</head>
<body>
    <h2>Tone-ner Wave Drawer</h2>
    <canvas id="waveCanvas" width="128" height="128"></canvas>

    <div class="controls">
        <div class="slider-container">
            <label for="smoothSlider">Smoothing:</label>
            <input type="range" id="smoothSlider" min="0" max="20" value="0" oninput="applySmoothing()">
            <span id="smoothVal">0</span>
        </div>

        <div class="btn-container">
            <button onclick="presetSine()">Sine</button>
            <button onclick="presetSquare()">Square</button>
            <button onclick="presetSaw()">Saw</button>
        </div>
    </div>

    <script>
        const canvas = document.getElementById('waveCanvas');
        const ctx = canvas.getContext('2d');
        
        // rawWave holds the exact original drawn points
        const rawWave = new Float32Array(128);
        // processedWave holds the smoothed output sent to ESP32
        const processedWave = new Float32Array(128);
        
        let drawing = false;
        let websocket = new WebSocket(`ws://${window.location.hostname}/ws`);

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
            if (websocket.readyState === WebSocket.OPEN) {
                websocket.send(processedWave.buffer);
            }
        }

        function applySmoothing() {
            const passes = parseInt(document.getElementById('smoothSlider').value);
            document.getElementById('smoothVal').innerText = passes;

            // Copy raw wave
            processedWave.set(rawWave);

            // Apply circular moving average filter for N passes
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

            // Write into rawWave array
            rawWave[x] = val;
            
            // Recalculate smoothing and transmit
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
    </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
        if (len == 128 * sizeof(float)) {
            memcpy(customWavetable, data, 128 * sizeof(float));
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
    // Populate initial sine wave
    for (int i = 0; i < 128; i++) {
        customWavetable[i] = sinf((2.0f * M_PI * i) / 128.0f);
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        waveVoices[i].padIndex = -1;
        waveVoices[i].amplitude = 0.0f;
    }

    // Initialize Wi-Fi Soft Access Point
    WiFi.softAP("Tone-ner-Synth");

    // Attach WebSockets handler
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // Serve HTML page
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

void wavetable_audio_process(int16_t *buffer, uint16_t touchMask) {
    ws.cleanupClients();

    for (uint8_t pad = 0; pad < 12; pad++) {
        if (touchMask & (1 << pad)) {
            bool playing = false;
            for (int v = 0; v < MAX_VOICES; v++) {
                if (waveVoices[v].padIndex == pad) { playing = true; break; }
            }
            if (!playing) {
                for (int v = 0; v < MAX_VOICES; v++) {
                    if (waveVoices[v].padIndex == -1) {
                        waveVoices[v].padIndex = pad;
                        waveVoices[v].baseFreq = noteFreqs[pad];
                        waveVoices[v].phase1 = 0.0f;
                        waveVoices[v].amplitude = 1.0f;
                        break;
                    }
                }
            }
        }
    }

    for (int v = 0; v < MAX_VOICES; v++) {
        if (waveVoices[v].padIndex != -1 && !(touchMask & (1 << waveVoices[v].padIndex))) {
            waveVoices[v].padIndex = -1;
            waveVoices[v].amplitude = 0.0f;
        }
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        float mixedSample = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            if (waveVoices[v].padIndex != -1) {
                float phaseInc = (128.0f * waveVoices[v].baseFreq) / SAMPLE_RATE;

                int index0 = (int)waveVoices[v].phase1;
                int index1 = (index0 + 1) % 128;
                float frac = waveVoices[v].phase1 - index0;

                float sample = customWavetable[index0] + frac * (customWavetable[index1] - customWavetable[index0]);
                mixedSample += sample * waveVoices[v].amplitude;

                waveVoices[v].phase1 += phaseInc;
                if (waveVoices[v].phase1 >= 128.0f) waveVoices[v].phase1 -= 128.0f;
            }
        }

        int16_t sample = (int16_t)(mixedSample * (MASTER_VOLUME / MAX_VOICES));
        buffer[i * 2]     = sample;
        buffer[i * 2 + 1] = sample;
    }
}