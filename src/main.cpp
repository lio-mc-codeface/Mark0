#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPR121.h>

#define I2C_SDA_PIN    21
#define I2C_SCL_PIN    22
#define MPR121_IRQ_PIN 5

Adafruit_MPR121 cap = Adafruit_MPR121();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\n--- MPR121 Force Sensitive Config ---");

  // Explicitly configure IRQ pin with internal pull-up
  pinMode(MPR121_IRQ_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(50000); // 50kHz for flex PCB stability

  // Initialize MPR121
  if (!cap.begin(0x5A, &Wire)) {
    Serial.println("MPR121 not found!");
    while (1);
  }

  // --- SAFE RE-CONFIGURATION SEQUENCE ---
  // Step 1: Put MPR121 into Stop Mode before changing electrode settings
  cap.writeRegister(MPR121_ECR, 0x00);

  // Step 2: Set sensitive touch/release thresholds (Touch: 5, Release: 2)
  cap.setThresholds(5, 2);

  // Step 3: Enable baseline tracking and set to Run Mode with all 12 electrodes (0x8F)
  // Bit 7:6 = 10 (Baseline tracking enabled, initial baseline = filtered data)
  // Bit 3:0 = 1111 (12 Electrodes enabled)
  cap.writeRegister(MPR121_ECR, 0x8F);

  Serial.println("Force configuration applied. Detecting pads...");
}

void loop() {
  // Read IRQ pin state (Active LOW when triggered)
  int irqState = digitalRead(MPR121_IRQ_PIN);

  Serial.println("\n--- Sensor Readings ---");
  Serial.print("IRQ Pin (GPIO 5): ");
  Serial.println(irqState == LOW ? "TRIGGERED (LOW)" : "IDLE (HIGH)");

  for (int i = 0; i < 12; i++) {
    uint16_t baseline = cap.baselineData(i);
    uint16_t filtered = cap.filteredData(i);
    int16_t diff = (int16_t)baseline - (int16_t)filtered;

    Serial.print("Pad "); Serial.print(i);
    Serial.print(" | Base: "); Serial.print(baseline);
    Serial.print(" | Filt: "); Serial.print(filtered);
    Serial.print(" | Diff: "); Serial.println(diff);
  }

  delay(1000);
}