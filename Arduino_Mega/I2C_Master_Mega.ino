#include <Wire.h>

const uint8_t SLAVES[] = {0x10, 0x11, 0x12, 0x13, 0x14}; // alamat 5 Nano
const uint8_t N_SLAVES = sizeof(SLAVES);

void setup() {
  Serial.begin(115200);
  Wire.begin();                 // Mega sebagai master
  // Wire.setClock(100000);     // pakai 100 kHz kalau kabel panjang
  Serial.println("Mega: siap menerima dari 5 slave...");
}

void loop() {
  static unsigned long t0 = 0;
  if (millis() - t0 >= 1000) {  // polling tiap 1 detik
    t0 = millis();

    for (uint8_t i = 0; i < N_SLAVES; i++) {
      uint8_t addr = SLAVES[i];

      // Minta maksimal 32 byte (batas Wire AVR)
      uint8_t n = Wire.requestFrom(addr, (uint8_t)8);
      if (n == 0) {
        Serial.print("0x"); if (addr < 16) Serial.print('0');
        Serial.print(addr, HEX);
        Serial.println(" : (no response)");
        continue;
      }

      char buf[33];
      uint8_t k = 0;
      while (Wire.available() && k < 32) buf[k++] = (char)Wire.read();
      buf[k] = '\0';

      Serial.print("0x"); if (addr < 16) Serial.print('0');
      Serial.print(addr, HEX);
      Serial.print(" : ");
      Serial.println(buf);
      delay(3); // jeda kecil antar slave
    }
    Serial.println();
  }
}
