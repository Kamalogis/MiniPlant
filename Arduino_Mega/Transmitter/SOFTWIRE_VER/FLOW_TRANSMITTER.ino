// === Import & Define (SW I2C OLED + HW I2C Slave) ===
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define SLAVE_ADDR     0x11
#define SPLASH_MS      200

#define FLOW_PIN       2                  // INT0 (D2)
#define FLOW_K         7.5f               // YF-S201: L/min = Hz / 7.5
#define FLOW_OFFSET   -0.1264f            // offset kalibrasi (opsional)
#define FLOW_GRADIENT  1.1399f

// OLED via Software I2C (pisah dari HW I2C A4/A5)
#define OLED_SCL_PIN   A2
#define OLED_SDA_PIN   A3

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE
);

// --- Flow & I2C ---
volatile unsigned long pulsa_sensor = 0;   // dihitung di ISR
unsigned long lastMillis = 0;
char msg[32] = "boot";

float debitLpm   = 0.0f;   // L/menit
float totalLiter = 0.0f;   // akumulasi liter

// ====== Area dinamis (dirty rectangles) ======
const int LPM_AREA_X = 0,   LPM_AREA_Y = 12, LPM_AREA_W = 128, LPM_AREA_H = 30;
const int LPM_BASELINE_Y = 40;

const int TOT_LABEL_X = 0,  TOT_LABEL_Y = 58;
const int TOT_AREA_X  = 42, TOT_AREA_Y  = 38, TOT_AREA_W  = 86, TOT_AREA_H  = 14;

// === ISR: hitung pulsa ===
void cacahPulsa() {
  pulsa_sensor++;
}

// === I2C handler ===
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
  int ascent  = u8g2.getAscent();
  int descent = u8g2.getDescent();
  int h = ascent - descent;

  int boxW = w + 2 * padX;
  if (boxW > u8g2.getDisplayWidth()) boxW = u8g2.getDisplayWidth();
  int x = (u8g2.getDisplayWidth() - boxW) / 2;
  int boxH = h + 2 * padY;

  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, y_top, boxW, boxH, radius);

  int tx = x + (boxW - w)/2;
  int ty = y_top + padY + ascent;
  u8g2.setDrawColor(0);
  u8g2.drawStr(tx, ty, txt);
  u8g2.setDrawColor(1);
}

// === Splash & Static ===
void renderSplash() {
  u8g2.clearBuffer();
  u8g2.drawHLine(0, 0, SCREEN_WIDTH);
  drawCentered("Water Treatment Plant", 12, u8g2_font_6x13_tr);
  drawCentered("By",                     26, u8g2_font_6x13_tr);
  u8g2.drawHLine(20, 34, SCREEN_WIDTH - 40);
  drawCenteredBadge("Kamalogis", 40, u8g2_font_10x20_mf, 6, 6, 3);
  u8g2.sendBuffer();
}

void renderStatic() {
  u8g2.clearBuffer();
  // Header
  drawCentered("Flow Transmitter", 10, u8g2_font_6x13_tr);

  // Label "Total :" dan "Pulsa/s:"
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(TOT_LABEL_X, TOT_LABEL_Y, "Total :");

  u8g2.sendBuffer();
}

// Hapus area kecil
void clearRect(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
}

// Update nilai (tanpa refresh penuh)
void updateDynamic(float lpm, float liters, float hz, unsigned long pulsa) {
  char buf[24];

  // 1) LPM besar
  clearRect(LPM_AREA_X, LPM_AREA_Y, LPM_AREA_W, LPM_AREA_H);
  u8g2.setFont(u8g2_font_logisoso20_tn);     // angka besar (0–9 .)
  dtostrf(lpm, 1, 2, buf);
  u8g2.drawStr(0, LPM_BASELINE_Y, buf);
  int x_unit = u8g2.getStrWidth(buf) + 4;
  u8g2.setFont(u8g2_font_9x15_tr);
  u8g2.drawStr(x_unit, LPM_BASELINE_Y - 5, " L/menit");

  // 2) Total liter
  clearRect(TOT_AREA_X, TOT_AREA_Y, TOT_AREA_W, TOT_AREA_H);
  u8g2.setFont(u8g2_font_6x13_tr);
  dtostrf(liters, 1, 3, buf);
  u8g2.drawStr(TOT_AREA_X, TOT_LABEL_Y, buf);
  int x_L = TOT_AREA_X + u8g2.getStrWidth(buf) + 3;
  u8g2.drawStr(x_L, TOT_LABEL_Y, " L");

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(9600);

  // I2C Slave
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  // Flow sensor interrupt
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), cacahPulsa, RISING);

  // OLED (SW I2C)
  u8g2.begin();
  renderSplash(); delay(SPLASH_MS);
  renderStatic();
  updateDynamic(0.0f, 0.0f, 0.0f, 0);
  lastMillis = millis();
}

void loop() {
  unsigned long now = millis();
  unsigned long dt_ms = now - lastMillis;

  if (dt_ms >= 1000) {                // update ~tiap 1 detik
    lastMillis = now;

    // Snapshot pulsa & reset
    noInterrupts();
    unsigned long pulsa = pulsa_sensor;
    pulsa_sensor = 0;
    interrupts();

    // Hitung frekuensi (Hz)
    float hz = 0.0f;
    if (dt_ms > 0) hz = (pulsa * 1000.0f) / (float)dt_ms;

    // L/menit (dengan kalibrasi linear)
    debitLpm = FLOW_GRADIENT * (hz / FLOW_K) + FLOW_OFFSET;
    if (debitLpm < 0) debitLpm = 0;

    // Akumulasi liter
    totalLiter += debitLpm * (dt_ms / 60000.0f);

    // Payload I2C (kirim LPM sebagai string)
    char vbuf[12];
    dtostrf(debitLpm, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);
    interrupts();

    // OLED (tanpa refresh penuh)
    updateDynamic(debitLpm, totalLiter, hz, pulsa);

    // Debug
    Serial.print(F("Hz: ")); Serial.print(hz, 2);
    Serial.print(F(" | LPM: ")); Serial.print(debitLpm, 3);
    Serial.print(F(" | Total L: ")); Serial.println(totalLiter, 3);
  }
}
