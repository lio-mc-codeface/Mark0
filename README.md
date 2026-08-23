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

# v4.0.0
New pinout to resolve conflicitng pins

pinout:

## ESP32-WROOM-32 30-Pin Development Board

This table is the current project pin map for the classic ESP32-WROOM-32 board
shown in the hardware documentation. GPIO 1 and GPIO 3 remain reserved for the
USB-UART programmer. GPIO 6-11 are reserved for the ESP32 module's internal
flash and are not available for project wiring.

| Pin # | Label | ESP32 Pin | Function |
| :-: | :-: | :-: | :--- |
| 1 | **3V3** | **3V3** | **3.3V system power rail for OLED, TFTs, SD card, MPR121, and INMP441** |
| 2 | **GND** | **GND** | **System ground rail** |
| 3 | **D15** | **GPIO 15** | **TFT2/ST7789 chip select; boot-strapping pin; TFT2 currently unused** |
| 4 | **D2** | **GPIO 2** | **Shared TFT hardware reset; boot-strapping pin** |
| 5 | **D4** | **GPIO 4** | **INMP441 I2S word select/LRCLK; reserved exclusively for microphone I2S** |
| 6 | **RX2** | **GPIO 16** | **Shared TFT data/command line; available on WROOM without PSRAM** |
| 7 | **TX2** | **GPIO 17** | **MAX98357A I2S audio data out** |
| 8 | **D5** | **GPIO 5** | **MPR121 active-low interrupt; boot-strapping pin** |
| 9 | **D18** | **GPIO 18** | **Shared SPI clock for GC9A01A, TFT2, and SD card** |
| 10 | **D19** | **GPIO 19** | **Shared SPI MISO; SD card data out** |
| 11 | **D21** | **GPIO 21** | **Shared I2C SDA for SSD1306 OLED and MPR121** |
| 12 | **RX0** | **GPIO 3** | **USB-UART serial RX; reserved for uploading and monitoring** |
| 13 | **TX0** | **GPIO 1** | **USB-UART serial TX; reserved for uploading and monitoring** |
| 14 | **D22** | **GPIO 22** | **Shared I2C SCL for SSD1306 OLED and MPR121** |
| 15 | **D23** | **GPIO 23** | **Shared SPI MOSI for GC9A01A, TFT2, and SD card** |
| 16 | **EN** | **EN** | **ESP32 reset input** |
| 17 | **VP** | **GPIO 36** | **Free; input-only; external pull-up required for future switch/button use** |
| 18 | **VN** | **GPIO 39** | **Free; input-only; external pull-up required for future switch/button use** |
| 19 | **D34** | **GPIO 34** | **Free; input-only; external pull-up required for future switch/button use** |
| 20 | **D35** | **GPIO 35** | **INMP441 I2S serial data input; input-only** |
| 21 | **D32** | **GPIO 32** | **Free; recommended future WS28xx LED data pin** |
| 22 | **D33** | **GPIO 33** | **Free; future control pin** |
| 23 | **D27** | **GPIO 27** | **INMP441 I2S bit clock/BCLK** |
| 24 | **D26** | **GPIO 26** | **MAX98357A I2S bit clock/BCLK** |
| 25 | **D25** | **GPIO 25** | **MAX98357A I2S word select/LRCLK** |
| 26 | **D14** | **GPIO 14** | **GC9A01A chip select; JTAG-capable but usable as normal GPIO** |
| 27 | **D12** | **GPIO 12** | **Currently unassigned; boot-strapping pin; avoid for new peripherals** |
| 28 | **D13** | **GPIO 13** | **SD card chip select** |
| 29 | **GND** | **GND** | **System ground rail** |
| 30 | **VIN** | **VIN** | **5V input rail for MAX98357A and other 5V peripherals** |

### Pinout Notes

- **INMP441:** connect VDD to 3V3, GND to GND, SCK to GPIO 27, WS to GPIO 4,
	SD to GPIO 35, and L/R to 3V3. L/R to 3V3 selects the right I2S slot,
	matching the current firmware configuration.
- **WS28xx LED:** do not use GPIO 4. GPIO 4 is the microphone WS clock.
	GPIO 32 is the recommended replacement, but the LED code and physical wire
	must be changed together later.
- **SPI and uploading:** GPIO 18, 19, 23, 13, and 14 can remain connected while
	uploading. They are separate from the ESP32 internal flash pins GPIO 6-11.
	The SD card and displays must remain deselected while the board resets.
- **TFT2/ST7789:** GPIO 15 is a boot-strapping pin. Delaying TFT2 initialization
	until after boot is possible, but it does not remove the need for TFT2 CS and
	attached circuitry to stay inactive during reset. TFT2 is currently unused.
- **GPIO 34, 36, and 39:** these pins are input-only and have no internal
	pull-ups. Add external pull-ups when future controls are connected.
- **Board assumption:** this map assumes the pictured classic ESP32-WROOM-32
	board without PSRAM. It should be rechecked if the physical board changes.