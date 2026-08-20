# Mark0
esp-synth
Release 0 has the flute with modulation tone generation + screen runing

# 🎹 ESP32 Synth - v3.0.0

## 🚀 What's New

### 🎙️ Live Sampler Mode
* **Live MEMS Audio Recording:** Hold **Pad 0** in Sampler mode to record directly from the INMP441 MEMS microphone over I2S (`I2S_NUM_1`).
* **Pitched Polyphonic Playback:** Play back recorded samples pitched across **Pads 1–11** with 4-voice polyphony.
* **Dynamic OLED Waveform Display:** Visualizes the loaded or newly recorded audio waveform in real time.

### ⚡ Performance & Memory
* **Dynamic Heap Allocation:** Replaced static `.bss` DRAM arrays with dynamic `malloc()` allocations to prevent FreeRTOS / ESP32 DRAM memory overflow (`dram0_0_seg`).
* **DC Bias Filter:** Integrated high-pass filtering and bit-shift alignment to capture crisp, low-noise audio from 24-bit MEMS microphone inputs.
<<<<<<< HEAD
=======

# v3.1.0
Added 1.28"screen + tested hello world
>>>>>>> b493714 (Update README for v0.2.0 release with GC9A01 screen support and sampler specs)

# v3.3.0
added master volume control
