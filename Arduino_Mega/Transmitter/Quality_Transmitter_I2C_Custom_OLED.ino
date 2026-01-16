// Import Important Library & Define Some Variabel
#include <EEPROM.h>
#include "GravityTDS.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SLAVE_ADDR    0x14
#define TdsSensorPin  A1
#define SPLASH_MS     200   // <<< atur durasi splash di sini (ms)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
GravityTDS gravityTds;

unsigned long lastMillis = 0;
char  msg[32] = "boot";
float temperature = 25.0f, tdsValue = 0.0f;

// ---------------- I2C onRequest: kirim isi msg ----------------
void onRequest() {
  uint8_t i = 0;
  while (msg[i] && i < sizeof(msg)) {
    Wire.write((uint8_t)msg[i]);
    i++;
  }
}

// ---------------- Helpers layout ----------------
void centerPrint(const String &txt, int16_t y, uint8_t size = 1) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - w) / 2;
  display.setCursor(x, y);
  display.print(txt);
}

void centerBadge(const String &txt, int16_t y, uint8_t size = 2, uint8_t radius = 6, uint8_t padX = 6, uint8_t padY = 2) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t boxW = w + 2*padX;
  int16_t boxH = h + 2*padY;
  if (boxW > display.width()) boxW = display.width();
  int16_t x = (display.width() - boxW) / 2;

  display.fillRoundRect(x, y, boxW, boxH, radius, SSD1306_WHITE);
  int16_t tx = x + (boxW - w) / 2;
  int16_t ty = y + (boxH - h) / 2;
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(tx, ty);
  display.print(txt);
  display.setTextColor(SSD1306_WHITE);
}

void renderSplash() {
  display.clearDisplay();
  display.drawLine(0, 0, SCREEN_WIDTH-1, 0, SSD1306_WHITE);
  centerPrint("Water Treatment Plant", 10, 1);
  centerPrint("By", 26, 1);
  display.drawFastHLine(20, 34, SCREEN_WIDTH-40, SSD1306_WHITE);
  centerBadge("Kamalogis", 40, 2, 6, 6, 3);
  display.display();
}

void renderMain(float tds_ppm) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Quality Transmitter"));

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(tds_ppm, 2);
  display.println(F(" ppm"));

  // Footer kecil: uptime
  display.setTextSize(0.6);
  display.setCursor(0, 45);
  display.print(F("Copyright: Kamalogis/2025"));

  display.display();  // <<< WAJIB untuk update OLED
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);        // ADC ref 5V (UNO)
  gravityTds.setAdcRange(1024);   // 10-bit ADC
  gravityTds.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // gagal init OLED
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // --- Splash singkat di setup ---
  renderSplash();
  delay(SPLASH_MS);          // <<< tampilkan splash sebentar

  lastMillis = millis();     // mulai timer untuk loop
}

// ---------------- Loop ----------------
void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 1000) {           // update tiap 1 detik
    lastMillis = now;

    //temperature = readTemperature();      // tambahkan sensor suhu jika ada
    gravityTds.setTemperature(temperature); // kompensasi suhu
    gravityTds.update();
    tdsValue = gravityTds.getTdsValue() - 35;  // offset kalibrasi (opsional)

    // siapkan pesan untuk I2C master
    char vbuf[12];
    dtostrf(tdsValue, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);   // salin string dengan aman
    interrupts();

    // render layar utama
    renderMain(tdsValue);

    // debug serial
    Serial.print((int)tdsValue);
    Serial.println(F("ppm"));
  }
}
