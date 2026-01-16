#include <Wire.h>
#include <U8g2lib.h>

#define trigPin        10
#define echoPin        9
#define TANK_HEIGHT_CM 35.0f
#define SLAVE_ADDR     0x12       // 0x12 dan 0x13 buat level
#define SPLASH_MS      200        // durasi splash di setup (ms)

// Posisi/ukuran ikon tangki di OLED
#define TANK_X         100
#define TANK_Y         10
#define TANK_W         20
#define TANK_H         50

// Gunakan hardware I2C, alamat default 0x3C
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,  // Rotasi layar (R0 = normal)
  /* clock=*/ 17, 
  /* data=*/ 16, 
  /* reset=*/ U8X8_PIN_NONE
);

unsigned long lastMillis = 0;
char msg[32] = "boot";

// ================== I2C: kirim buffer msg ==================
void onRequest() {
  uint8_t i = 0;
  while (msg[i] && i < sizeof(msg)) {
    Wire.write((uint8_t)msg[i]);
    i++;
  }
}

// ================== Sensor jarak (cm) ==================
float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long dur = pulseIn(echoPin, HIGH, 30000UL); // 30 ms ~ >5 m
  if (dur == 0) return TANK_HEIGHT_CM; // anggap kosong jika timeout
  float distance = dur * 0.0343f / 2.0f;
  return distance;
}

// ================== Helpers tampilan ==================
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

// ================== Splash & Main ==================
void renderSplash() {
  u8g2.clearBuffer();
  u8g2.drawHLine(0, 0, u8g2.getDisplayWidth());
  centerPrint("Water Treatment Plant", 15, u8g2_font_6x10_tr);
  centerPrint("By", 30, u8g2_font_6x10_tr);
  u8g2.drawHLine(20, 38, u8g2.getDisplayWidth() - 40);
  centerBadge("Kamalogis", 55, u8g2_font_7x13B_tr);
  u8g2.sendBuffer();
}

void renderMain(float level_cm, float percent, float distance_cm) {
  u8g2.clearBuffer();

  // Header
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "Level Transmitter");

  // Persentase besar
  u8g2.setFont(u8g2_font_fub14_tr);
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f %%", percent);
  u8g2.drawStr(0, 30, buf);

  // Info kecil
  u8g2.setFont(u8g2_font_6x10_tr);
  snprintf(buf, sizeof(buf), "Level : %.1f cm", level_cm);
  u8g2.drawStr(0, 45, buf);
  snprintf(buf, sizeof(buf), "Head  : %.1f cm", distance_cm);
  u8g2.drawStr(0, 58, buf);

  // Tangki kanan
  u8g2.drawFrame(TANK_X, TANK_Y, TANK_W, TANK_H);
  int fillHeight = (int)((percent / 100.0f) * TANK_H);
  if (fillHeight < 0) fillHeight = 0;
  if (fillHeight > TANK_H) fillHeight = TANK_H;
  int fillY = TANK_Y + TANK_H - fillHeight;
  if (fillHeight > 0) u8g2.drawBox(TANK_X + 1, fillY, TANK_W - 2, fillHeight);

  // Tick mark tiap 10%
  for (int i = 0; i <= 10; i++) {
    int y = TANK_Y + TANK_H - (i * TANK_H) / 10;
    u8g2.drawHLine(TANK_X - 3, y, 3);
  }

  u8g2.sendBuffer();
}

// ================== Setup & Loop ==================
void setup() {
  Serial.begin(9600);
  Serial.println("Amar");

  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);

  u8g2.begin();
  Serial.println("OLED U8g2 aktif");

  renderSplash();
  delay(SPLASH_MS);

  lastMillis = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 500) { // refresh ~0.5 detik
    lastMillis = now;

    float distance = getDistance();
    if (distance > TANK_HEIGHT_CM) distance = TANK_HEIGHT_CM;
    if (distance < 0) distance = 0;

    float level   = TANK_HEIGHT_CM - distance;
    float percent = (level / TANK_HEIGHT_CM) * 100.0f;

    // Payload I2C (kirim distance dalam cm)
    char vbuf[12];
    dtostrf(distance, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);
    interrupts();

    // Debug
    Serial.print(F("Tinggi air: "));
    Serial.print(level, 1);
    Serial.print(F(" cm ("));
    Serial.print(percent, 1);
    Serial.println(F(" %)"));

    // OLED
    renderMain(level, percent, distance);
  }
}
