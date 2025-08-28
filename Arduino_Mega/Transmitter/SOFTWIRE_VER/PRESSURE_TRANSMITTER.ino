// === Import & Define (SW I2C for OLED + HW I2C for Slave) ===
#include <Wire.h>
#include <U8g2lib.h>

#define SLAVE_ADDR 0x10
#define PT_PIN A0
#define VREF_V 5.0f
#define SPLASH_MS 200

// Skala tekanan untuk progress bar
#define P_MIN_BAR 0.0f
#define P_MAX_BAR 12.0f

// OLED via Software I2C (pisah dari HW I2C A4/A5)
#define OLED_SCL_PIN A3
#define OLED_SDA_PIN A2

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);

unsigned long lastMillis = 0;
char msg[32] = "boot";

// ====== Area dinamis (dirty rectangles) ======
const int VAL_X = 4;
const int VAL_BASELINE_Y = 28;              // turun sedikit
const int VAL_AREA_X = 0, VAL_AREA_Y = 10;  // area angka diperkecil
const int VAL_AREA_W = 128, VAL_AREA_H = 16;

const int BAR_W = 110;                // diperkecil dari 120 -> 110
const int BAR_H = 6;                  // dipipihkan dari 8 -> 6
const int BAR_X = (128 - BAR_W) / 2;  // center align
const int BAR_Y = 34;

const int VLT_X = 0, VLT_Y = 54;
const int VLT_AREA_X = 58, VLT_AREA_Y = 42, VLT_AREA_W = 70, VLT_AREA_H = 14;

// === I2C handler: kirim isi msg (pressure sebagai string) ===
void onRequest() {
  uint8_t i = 0;
  while (msg[i] && i < sizeof(msg)) {
    Wire.write((uint8_t)msg[i]);
    i++;
  }
}

// === Helpers layout (U8g2) ===
int16_t centerX(const char *txt, const uint8_t *font) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(txt);
  return (u8g2.getDisplayWidth() - w) / 2;
}

void drawCentered(const char *txt, int16_t y, const uint8_t *font) {
  u8g2.setFont(font);
  u8g2.drawStr(centerX(txt, font), y, txt);
}

void drawCenteredBadge(const char *txt, int16_t y_top,
                       const uint8_t *font,
                       uint8_t radius = 6, uint8_t padX = 6, uint8_t padY = 2) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(txt);
  int ascent = u8g2.getAscent();
  int descent = u8g2.getDescent();
  int h = ascent - descent;

  int boxW = w + 2 * padX;
  if (boxW > u8g2.getDisplayWidth()) boxW = u8g2.getDisplayWidth();
  int x = (u8g2.getDisplayWidth() - boxW) / 2;
  int boxH = h + 2 * padY;

  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, y_top, boxW, boxH, radius);

  int tx = x + (boxW - w) / 2;
  int ty = y_top + padY + ascent;
  u8g2.setDrawColor(0);
  u8g2.drawStr(tx, ty, txt);
  u8g2.setDrawColor(1);
}

// === Splash & Static ===
void renderSplash() {
  u8g2.clearBuffer();
  u8g2.drawHLine(0, 0, 128);
  drawCentered("Water Treatment Plant", 12, u8g2_font_6x13_tr);
  drawCentered("By", 26, u8g2_font_6x13_tr);
  u8g2.drawHLine(20, 34, 128 - 40);
  drawCenteredBadge("Kamalogis", 40, u8g2_font_10x20_mf, 6, 6, 3);
  u8g2.sendBuffer();
}

// Gambar frame statis SEKALI (header, outline bar, label)
void renderStatic() {
  u8g2.clearBuffer();
  // Header
  drawCentered("Pressure Transmitter", 10, u8g2_font_6x13_tr);

  // Outline progress bar (pakai ukuran baru)
  u8g2.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);

  // Label "Tegangan:"
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(VLT_X, VLT_Y, "Tegangan:");

  u8g2.sendBuffer();
}

// Hapus area kecil (kotak)
void clearRect(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
}

// Update nilai pressure, bar, volt TANPA refresh penuh
void updateDynamic(float pressure_bar, float volt) {
  // 1) Angka besar (bar) — lebih kecil
  clearRect(VAL_AREA_X, VAL_AREA_Y, VAL_AREA_W, VAL_AREA_H);
  u8g2.setFont(u8g2_font_fub14_tn);  // lebih kecil dari logisoso20
  char nb[16];
  dtostrf(pressure_bar, 1, 2, nb);
  u8g2.drawStr(VAL_X, VAL_BASELINE_Y, nb);

  // Unit "bar"
  int x_bar = u8g2.getStrWidth(nb) + 4;
  u8g2.setFont(u8g2_font_9x15_tr);
  u8g2.drawStr(x_bar, VAL_BASELINE_Y, " bar");

  // 2) Progress bar (ukuran baru)
  clearRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2);
  float clamped = pressure_bar;
  if (clamped < P_MIN_BAR) clamped = P_MIN_BAR;
  if (clamped > P_MAX_BAR) clamped = P_MAX_BAR;
  int fillW = (int)((clamped - P_MIN_BAR) * (BAR_W - 2) / (P_MAX_BAR - P_MIN_BAR));
  if (fillW > 0) u8g2.drawBox(BAR_X + 1, BAR_Y + 1, fillW, BAR_H - 2);

  // 3) Volt line → hanya nilainya
  clearRect(VLT_AREA_X, VLT_AREA_Y, VLT_AREA_W, VLT_AREA_H);
  u8g2.setFont(u8g2_font_6x13_tr);
  char vb[16];
  dtostrf(volt, 1, 3, vb);
  u8g2.drawStr(VLT_AREA_X, VLT_Y, vb);
  u8g2.drawStr(VLT_AREA_X + u8g2.getStrWidth(vb) + 3, VLT_Y, " V");

  // Kirim perubahan
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(9600);

  // I2C Slave (HW I2C pada A4/A5)
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  // OLED (SW I2C) mulai
  u8g2.begin();
  renderSplash();
  delay(SPLASH_MS);
  renderStatic();
  updateDynamic(0.0f, 0.0f);
  lastMillis = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 1000) {  // update tiap 1 detik
    lastMillis = now;

    // Baca ADC & hitung tegangan
    int adc = analogRead(PT_PIN);           // 10-bit: 0..1023
    float volt = (adc * VREF_V) / 1023.0f;  // Volt

    // Linear calibration (ganti sesuai regresi kamu)
    float pressure = volt * 3.040784f - 0.560074f;

    // Siapkan string untuk I2C master
    char vbuf[12];
    dtostrf(pressure, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);  // kirim pressure
    interrupts();

    // Update OLED (tanpa refresh penuh)
    updateDynamic(pressure, volt);

    // Debug serial
    Serial.print(F("V="));
    Serial.print(volt, 3);
    Serial.print(F("  P="));
    Serial.println(pressure, 3);
  }
}
