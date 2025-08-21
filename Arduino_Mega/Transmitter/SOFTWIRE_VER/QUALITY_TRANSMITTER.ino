// ===== Import Library & Define Variables =====
#include <EEPROM.h>
#include "GravityTDS.h"
#include <Wire.h>
#include <U8g2lib.h>    // <— ganti ke U8g2 (software I2C)

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define SLAVE_ADDR    0x14
#define TdsSensorPin    A1
#define SPLASH_MS      200   // durasi splash (ms)

// === OLED via Software I2C (pisah dari HW I2C A4/A5)
#define OLED_SCL_PIN    A2    // UBAH jika perlu
#define OLED_SDA_PIN    A3    // UBAH jika perlu

// Full-buffer mode (F) + SW I2C; reset=U8X8_PIN_NONE
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,           // rotasi
  /* clock=*/ OLED_SCL_PIN,
  /* data =*/ OLED_SDA_PIN,
  /* reset=*/ U8X8_PIN_NONE
);

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

// ==== Area nilai (dirty rectangle) ====
const int VAL_AREA_X = 0;
const int VAL_AREA_Y = 24;      // top area angka
const int VAL_AREA_W = 128;
const int VAL_AREA_H = 38;      // cukup utk font 20–24pt + "ppm"
const int VAL_BASELINE_Y = 46;  // baseline angka


// ---------------- Helpers layout (U8g2) ----------------
int16_t centerX(const char *txt) {
  int w = u8g2.getStrWidth(txt);
  return (u8g2.getDisplayWidth() - w) / 2;
}

// gambar teks terpusat (berbasis baseline Y)
void drawCentered(const char *txt, int16_t y, const uint8_t *font) {
  u8g2.setFont(font);
  u8g2.drawStr(centerX(txt), y, txt);
}

// badge terpusat dengan rounded box
void drawCenteredBadge(const char *txt, int16_t y_top,
                       const uint8_t *font,
                       uint8_t radius = 6, uint8_t padX = 6, uint8_t padY = 2) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(txt);
  int ascent  = u8g2.getAscent();
  int descent = u8g2.getDescent();       // negatif
  int h = ascent - descent;              // tinggi font (px)

  int boxW = w + 2 * padX;
  if (boxW > u8g2.getDisplayWidth()) boxW = u8g2.getDisplayWidth();
  int x = (u8g2.getDisplayWidth() - boxW) / 2;
  int boxH = h + 2 * padY;

  // kotak putih
  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, y_top, boxW, boxH, radius);

  // teks hitam
  int tx = x + (boxW - w) / 2;
  int ty = y_top + padY + ascent;        // baseline = top + padY + ascent
  u8g2.setDrawColor(0);
  u8g2.drawStr(tx, ty, txt);

  // kembalikan warna gambar ke default (putih)
  u8g2.setDrawColor(1);
}

void renderSplash() {
  u8g2.clearBuffer();
  // garis top
  u8g2.drawHLine(0, 0, SCREEN_WIDTH);

  // judul
  drawCentered("Water Treatment Plant", 12, u8g2_font_6x13_tr);
  drawCentered("By",                     26, u8g2_font_6x13_tr);

  // garis pemisah
  u8g2.drawHLine(20, 34, SCREEN_WIDTH - 40);

  // badge
  drawCenteredBadge("Kamalogis", 40, u8g2_font_10x20_mf, 6, 6, 3);

  u8g2.sendBuffer();
}
void renderStatic() {
  u8g2.clearBuffer();
  drawCentered("Quality Transmitter", 12, u8g2_font_6x13_tr);
  u8g2.sendBuffer();
}

void updateValue(float tds_ppm) {
  // tampilkan 1 desimal biar stabil
  float disp = roundf(tds_ppm * 10.0f) / 10.0f;
  static float lastDisp = -1.0f;
  if (fabs(disp - lastDisp) < 0.1f) return; // skip kalau beda < 0.1

  char vbuf[24];
  dtostrf(disp, 1, 1, vbuf);  // contoh: "123.4"

  // bersihkan HANYA area angka
  u8g2.setDrawColor(0);
  u8g2.drawBox(VAL_AREA_X, VAL_AREA_Y, VAL_AREA_W, VAL_AREA_H);
  u8g2.setDrawColor(1);

  // angka besar → gunakan font numeric-only (punya digit & titik)
  u8g2.setFont(u8g2_font_logisoso24_tn);   // atau u8g2_font_fub20_tn
  u8g2.drawStr(0, VAL_BASELINE_Y, vbuf);

  // unit
  int x_ppm = u8g2.getStrWidth(vbuf) + 4;
  u8g2.setFont(u8g2_font_9x15_tr);
  u8g2.drawStr(x_ppm, VAL_BASELINE_Y, "ppm");

  u8g2.sendBuffer();
  lastDisp = disp;
}


// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);

  // I2C Slave pada HW I2C
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  // TDS
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);        // ADC ref 5V (UNO)
  gravityTds.setAdcRange(1024);   // 10-bit ADC
  gravityTds.begin();

  u8g2.begin();
  renderSplash(); delay(SPLASH_MS);
  renderStatic();
  lastMillis = millis();
  updateValue(0);   // nilai awal
}

// ---------------- Loop ----------------
void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 1000) {           // update tiap 1 detik
    lastMillis = now;

    //temperature = readTemperature();      // tambahkan sensor suhu jika ada
    gravityTds.setTemperature(temperature); // kompensasi suhu
    gravityTds.update();

    // Kalibrasi/offset opsional
    tdsValue = 1.1668f * gravityTds.getTdsValue() - 45.493f;
    if (tdsValue < 0) tdsValue = 0;

    // siapkan pesan untuk I2C master
    char vbuf[12];
    dtostrf(tdsValue, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);   // salin string aman
    interrupts();

    // render layar utama
    updateValue(tdsValue);

    // debug serial
    Serial.print((int)tdsValue);
    Serial.println(F("ppm"));
  }
}
