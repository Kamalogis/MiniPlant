// === Import & Define ===
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SLAVE_ADDR    0x10
#define PT_PIN        A0
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define VREF_V        5.0f
#define SPLASH_MS     200   // << atur durasi splash di setup (ms)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
unsigned long lastMillis = 0;
char msg[32] = "boot";

// === I2C handler: kirim isi msg ===
void onRequest() {
  uint8_t i = 0;
  while (msg[i] && i < sizeof(msg)) {
    Wire.write((uint8_t)msg[i]);
    i++;
  }
}

// === Helpers untuk splash yang rapi ===
void centerPrint(const String &txt, int16_t y, uint8_t size = 1) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - w) / 2;
  display.setCursor(x, y);
  display.print(txt);
}

void centerBadge(const String &txt, int16_t y, uint8_t size = 2,
                 uint8_t radius = 6, uint8_t padX = 6, uint8_t padY = 3) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t boxW = w + 2*padX;
  int16_t boxH = h + 2*padY;
  if (boxW > display.width()) boxW = display.width();
  int16_t x = (display.width() - boxW) / 2;

  display.fillRoundRect(x, y, boxW, boxH, radius, SSD1306_WHITE);
  int16_t tx = x + (boxW - w)/2;
  int16_t ty = y + (boxH - h)/2;
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

void renderMain(float pressure_bar, float volt) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Pressure Transmitter"));

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(pressure_bar, 2);
  display.println(F(" bar"));

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(F("Tegangan: "));
  display.print(volt, 3);
  display.println(F(" V"));

  // indikator uptime kecil di pojok bawah
  display.setCursor(0, 54);
  display.print(F("Uptime "));
  display.print(millis()/1000);
  display.print(F(" s"));

  display.display();
}

// === Setup ===
void setup() {
  Serial.begin(9600);
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // gagal init OLED
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Splash singkat di setup
  renderSplash();
  delay(SPLASH_MS);

  // Tampilkan layar utama pertama kali (nilai awal 0)

  lastMillis = millis();
}

// === Loop ===
void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 1000) {   // update setiap 1 detik
    lastMillis = now;

    // Baca ADC & hitung tegangan
    int   adc   = analogRead(PT_PIN);          // 10-bit: 0..1023
    float volt  = (adc * VREF_V) / 1023.0f;    // Volt
    // Linear calibration (ganti sesuai regresi kamu)
    float pressure = volt * 3.040784f - 0.560074f;

    // Siapkan string untuk I2C master
    char vbuf[12];
    dtostrf(pressure, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);    // salin aman
    interrupts();

    // Render OLED
    renderMain(pressure, volt);

    // Debug serial
    Serial.print(F("Tegangan : "));  Serial.println(volt, 3);
    Serial.print(F("Pressure : "));  Serial.println(pressure, 3);
  }
}
