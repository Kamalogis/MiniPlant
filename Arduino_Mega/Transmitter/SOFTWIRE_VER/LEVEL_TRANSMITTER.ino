#include <Wire.h>
#include <U8g2lib.h>

#define trigPin        10
#define echoPin        9
#define TANK_HEIGHT_CM 30.0f
#define SLAVE_ADDR     0x13 //0x12 dan 0x13 
#define SPLASH_MS      200

// Posisi/ukuran ikon tangki di OLED
#define TANK_X         100
#define TANK_Y         10
#define TANK_W         20
#define TANK_H         50

// OLED via Software I2C (pisah dari HW I2C A4/A5)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,  /*clock=*/A3, /*data=*/A2, /*reset=*/U8X8_PIN_NONE
);

unsigned long lastMillis = 0;
char msg[32] = "boot";

// ====== Area dinamis (dirty rectangles) ======
const int PCT_AREA_X = 0,  PCT_AREA_Y = 20, PCT_AREA_W = 96, PCT_AREA_H = 16;
const int PCT_BASELINE_Y = 34;

const int LVL_LABEL_X = 0, LVL_LABEL_Y = 45;
const int LVL_VAL_X   = 48, LVL_VAL_Y   = 45;
const int LVL_VAL_W   = 35, LVL_VAL_H   = 12;

const int HD_LABEL_X  = 0, HD_LABEL_Y  = 58;
const int HD_VAL_X    = 48, HD_VAL_Y    = 58;
const int HD_VAL_W    = 35, HD_VAL_H    = 12;

float prevDistance = TANK_HEIGHT_CM;   // untuk smoothing

// ================== I2C: kirim buffer msg ==================
void onRequest() {
  uint8_t i = 0;
  while (msg[i] && i < sizeof(msg)) {
    Wire.write((uint8_t)msg[i]);
    i++;
  }
}

// ================== Sensor jarak (cm) ==================
float getDistanceOnce() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long dur = pulseIn(echoPin, HIGH, 30000UL); // 30 ms timeout
  if (dur == 0) return TANK_HEIGHT_CM;                 // anggap kosong jika timeout
  return dur * 0.0343f * 0.5f;                         // cm
}

// ================== Helpers tampilan ==================
void clearRect(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
}

void centerPrint(const char *txt, int16_t y, const uint8_t *font = u8g2_font_6x10_tr) {
  u8g2.setFont(font);
  int16_t w = u8g2.getStrWidth(txt);
  int16_t x = (u8g2.getDisplayWidth() - w) / 2;
  u8g2.drawStr(x, y, txt);
}

void centerBadge(const char *txt, int16_t y, const uint8_t *font = u8g2_font_7x13B_tr) {
  u8g2.setFont(font);
  int16_t w = u8g2.getStrWidth(txt);
  int16_t h = u8g2.getMaxCharHeight();
  int16_t boxW = w + 12;
  int16_t boxH = h + 6;
  int16_t x = (u8g2.getDisplayWidth() - boxW) / 2;

  u8g2.drawRBox(x, y - h, boxW, boxH, 4);
  int16_t tx = x + (boxW - w) / 2;
  int16_t ty = y;
  u8g2.setDrawColor(0);
  u8g2.drawStr(tx, ty, txt);
  u8g2.setDrawColor(1);
}

// ================== Splash & Static ==================
void renderSplash() {
  u8g2.clearBuffer();
  u8g2.drawHLine(0, 0, u8g2.getDisplayWidth());
  centerPrint("Water Treatment Plant", 15, u8g2_font_6x10_tr);
  centerPrint("By", 30, u8g2_font_6x10_tr);
  u8g2.drawHLine(20, 38, u8g2.getDisplayWidth() - 40);
  centerBadge("Kamalogis", 55, u8g2_font_7x13B_tr);
  u8g2.sendBuffer();
}

// Gambar elemen statis SEKALI (header, frame tangki, tick, label)
void renderStatic() {
  u8g2.clearBuffer();

  // Header
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "Level Transmitter");

  // Frame & tick tank kanan
  u8g2.drawFrame(TANK_X, TANK_Y, TANK_W, TANK_H);
  for (int i = 0; i <= 10; i++) {
    int y = TANK_Y + TANK_H - (i * TANK_H) / 10;
    u8g2.drawHLine(TANK_X - 3, y, 3);
  }

  // Label statis kiri
  u8g2.drawStr(LVL_LABEL_X, LVL_LABEL_Y, "Level:");
  u8g2.drawStr(HD_LABEL_X,  HD_LABEL_Y,  "Head :");

  u8g2.sendBuffer();
}

void updateDynamic(float level_cm, float percent, float distance_cm) {
  // 1) Persentase besar
  clearRect(PCT_AREA_X, PCT_AREA_Y, /*lebarkan*/ 80, PCT_AREA_H);
  u8g2.setFont(u8g2_font_fub14_tr);
  u8g2.setCursor(0, PCT_BASELINE_Y);
  u8g2.print(percent, 1);   // <<<< INI aman (tanpa %f)
  u8g2.print(" %");

  // 2) Level (cm)
  clearRect(LVL_VAL_X, LVL_VAL_Y - 10, /*lebarkan*/ 40, LVL_VAL_H);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(LVL_VAL_X, LVL_VAL_Y);
  u8g2.print(level_cm, 1);  // <<<< aman
  u8g2.print(" cm");

  // 3) Head (cm)
  clearRect(HD_VAL_X, HD_VAL_Y - 10, /*lebarkan*/ 40, HD_VAL_H);
  u8g2.setCursor(HD_VAL_X, HD_VAL_Y);
  u8g2.print(distance_cm, 1); // <<<< aman
  u8g2.print(" cm");

  // 4) Isi tangki (seperti semula)
  clearRect(TANK_X + 1, TANK_Y + 1, TANK_W - 2, TANK_H - 2);
  int fillHeight = (int)((percent / 100.0f) * TANK_H);
  if (fillHeight < 0) fillHeight = 0;
  if (fillHeight > TANK_H) fillHeight = TANK_H;
  int fillY = TANK_Y + TANK_H - fillHeight;
  if (fillHeight > 0) u8g2.drawBox(TANK_X + 1, fillY, TANK_W - 2, fillHeight);

  u8g2.sendBuffer();
}

// ================== Setup & Loop ==================
void setup() {
  Serial.begin(9600);

  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);

  u8g2.begin();
  renderSplash(); delay(SPLASH_MS);
  renderStatic();
  lastMillis = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 500) {            // refresh ~0.5 s
    unsigned long dt_ms = now - lastMillis;
    lastMillis = now;

    // Baca & smooth (exponential moving average)
    float raw = getDistanceOnce();
    const float alpha = 0.30f;  // 0..1 (semakin kecil = lebih halus)
    float distance = alpha * raw + (1.0f - alpha) * prevDistance;
    prevDistance = distance;

    // Clamp ke tinggi tangki
    if (distance > TANK_HEIGHT_CM) distance = TANK_HEIGHT_CM;
    if (distance < 0) distance = 0;

    float level   = TANK_HEIGHT_CM - distance;
    float percent = (level / TANK_HEIGHT_CM) * 100.0f;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    // Payload I2C (kirim distance cm)
    char vbuf[12];
    dtostrf(percent, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);
    interrupts();

    // Debug
    Serial.print(F("Level: "));
    Serial.print(level, 1);
    Serial.print(F(" cm ("));
    Serial.print(percent, 1);
    Serial.println(F("%)"));

    // OLED: hanya update area dinamis
    updateDynamic(level, percent, distance);
  }
}
