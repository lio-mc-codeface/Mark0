# Mark0
esp-synth
Release 0 has the flute with modulation tone generation + screen runing

# ESP32 Synth - v4.4.0

## 🚀 What's New

### 🎙️ Live Sampler Mode
* **Live MEMS Audio Recording:** Hold **Pad 0** in Sampler mode to record directly from the INMP441 MEMS microphone over I2S (`I2S_NUM_1`).
* **Pitched Polyphonic Playback:** Play back recorded samples pitched across **Pads 1–11** with 4-voice polyphony.
* **Dynamic OLED Waveform Display:** Visualizes the loaded or newly recorded audio waveform in real time.

### ⚡ Performance & Memory
* **Dynamic Heap Allocation:** Replaced static `.bss` DRAM arrays with dynamic `malloc()` allocations to prevent FreeRTOS / ESP32 DRAM memory overflow (`dram0_0_seg`).
* **DC Bias Filter:** Integrated high-pass filtering and bit-shift alignment to capture crisp, low-noise audio from 24-bit MEMS microphone inputs.
# v3.3.0
added master volume control

# v4.0.0
New pinout to resolve conflicitng pins

## Current Pinout

| Physical pin | Board label | ESP32 GPIO | Connected hardware |
| :---: | :--- | :---: | :--- |
| 1 | 3V3 | 3V3 | 3.3 V power rail |
| 2 | GND | GND | Ground |
| 3 | D15 | 15 | ST7735 CS; boot-strapping pin |
| 4 | D2 | 2 | GC9A01A reset; boot-strapping pin |
| 5 | D4 | 4 | INMP441 WS/LRCLK |
| 6 | RX2 | 16 | Shared TFT DC |
| 7 | TX2 | 17 | MAX98357A and PCM5102 audio data |
| 8 | D5 | 5 | MPR121 active-low IRQ |
| 9 | D18 | 18 | Shared SPI SCLK |
| 10 | D19 | 19 | Shared SPI MISO; SD card input |
| 11 | D21 | 21 | I2C SDA; SSD1306 and MPR121 |
| 12 | RX0 | 3 | USB-UART RX; reserved |
| 13 | TX0 | 1 | USB-UART TX; reserved |
| 14 | D22 | 22 | I2C SCL; SSD1306 and MPR121 |
| 15 | D23 | 23 | Shared SPI MOSI |
| 16 | EN | EN | ESP32 reset input |
| 17 | VP | 36 | Encoder B; input-only |
| 18 | VN | 39 | Encoder A; input-only |
| 19 | D34 | 34 | Encoder push button; input-only |
| 20 | D35 | 35 | INMP441 data; input-only |
| 21 | D32 | 32 | BUTTON1/BUTTON2 resistor-ladder ADC |
| 22 | D33 | 33 | ST7735 reset |
| 23 | D27 | 27 | INMP441 SCK/BCLK |
| 24 | D26 | 26 | MAX98357A and PCM5102 BCLK |
| 25 | D25 | 25 | MAX98357A and PCM5102 LRCLK |
| 26 | D14 | 14 | GC9A01A CS |
| 27 | D12 | 12 | WS2812 LED data |
| 28 | D13 | 13 | SD card CS |
| 29 | GND | GND | Ground |
| 30 | VIN | VIN | 5 V input rail |

### Pinout Notes

- **INMP441:** connect VDD to 3V3, GND to GND, SCK to GPIO 27, WS to GPIO 4,
	SD to GPIO 35, and L/R to 3V3. L/R to 3V3 selects the right I2S slot,
	matching the current firmware configuration.
- **WS28xx LED:** data is on GPIO 12. The `DOUT` header is intentionally not
  connected yet and is reserved for future daisy chaining.
- **SPI and uploading:** GPIO 18, 19, 23, 13, and 14 can remain connected while
	uploading. They are separate from the ESP32 internal flash pins GPIO 6-11.
	The SD card and displays must remain deselected while the board resets.
- **TFT2/ST7735:** GPIO 15 is the chip select and GPIO 33 is its separate reset.
	The display can be initialized and operated independently while sharing SPI
	clock, MOSI, and data/command with the GC9A01. Keep GPIO 15 inactive during
	reset because it is a boot-strapping pin.
- **GPIO 34, 36, and 39:** these pins are input-only and have no internal
	pull-ups. The encoder uses external 10 kOhm resistors.
- **Button ladder:** GPIO 32 uses a 10 kOhm pull-up, with 10 kOhm for BUTTON1
	and 20 kOhm for BUTTON2. The firmware applies a 30 ms debounce window.
- **Board assumption:** this map assumes the pictured classic ESP32-WROOM-32
	board without PSRAM. It should be rechecked if the physical board changes.

	24-8-
	I will change the irq addres of the mrn121 to the scl pin making it 0x5D, this wil free up other adreses of other mrn's in the future, since im low on inputs anyways

## v4.4.0 screen from 7735->7789
## v4.5.0 boot screen for the 7789 i belive
## v4.6.1 – Pop Eradicated

### Fixed
- Soft attack & soft release on all synth modes (Flute, Wavetable, Sampler)
- Eliminated note-on / note-off clicks and pops
- Removed volume jumps when adding extra notes in Wavetable mode
- Improved overall polyphony cleanliness