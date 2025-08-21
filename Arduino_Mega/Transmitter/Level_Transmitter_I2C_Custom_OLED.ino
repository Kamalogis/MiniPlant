#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1

#define trigPin        10
#define echoPin        9
#define TANK_HEIGHT_CM 35.0f
#define SLAVE_ADDR     0x13       // 0x12 dan 0x13 buat level
#define SPLASH_MS      200        // durasi splash di setup (ms)

// Posisi/ukuran ikon tangki di OLED
#define TANK_X         100
#define TANK_Y         10
#define TANK_W         20
#define TANK_H         50

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
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

  // Timeout ringan biar nggak ngegantung
  unsigned long dur = pulseIn(echoPin, HIGH, 30000UL); // 30 ms ~ >5 m
  if (dur == 0) return TANK_HEIGHT_CM; // anggap kosong jika timeout
  float distance = dur * 0.0343f / 2.0f;
  return distance;
}

// ================== Helpers tampilan ==================
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

// ================== Splash & Main ==================
void renderSplash() {
  display.clearDisplay();
  display.drawLine(0, 0, SCREEN_WIDTH-1, 0, SSD1306_WHITE);
  centerPrint("Water Treatment Plant", 10, 1);
  centerPrint("By", 26, 1);
  display.drawFastHLine(20, 34, SCREEN_WIDTH-40, SSD1306_WHITE);
  centerBadge("Kamalogis", 40, 2, 6, 6, 3);
  display.display();
}

void renderMain(float level_cm, float percent, float distance_cm) {
  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Level Transmitter"));

  // Persentase besar
  display.setTextSize(2);
  display.setCursor(8, 16);
  display.print(percent, 1);
  display.println(F(" %"));

  // Info kecil
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print(F("Level : "));
  display.print(level_cm, 1);
  display.println(F(" cm"));

  display.setCursor(0, 52);
  display.print(F("Head  : "));
  display.print(distance_cm, 1);
  display.println(F(" cm"));

  // Tangki kanan
  display.drawRect(TANK_X, TANK_Y, TANK_W, TANK_H, SSD1306_WHITE);
  int fillHeight = (int)((percent / 100.0f) * TANK_H);
  if (fillHeight < 0) fillHeight = 0;
  if (fillHeight > TANK_H) fillHeight = TANK_H;
  int fillY = TANK_Y + TANK_H - fillHeight;
  if (fillHeight > 0) display.fillRect(TANK_X + 1, fillY, TANK_W - 2, fillHeight, SSD1306_WHITE);

  // Tick mark tiap 10%
  for (int i = 0; i <= 10; i++) {
    int y = TANK_Y + TANK_H - (i * TANK_H) / 10;
    display.drawFastHLine(TANK_X - 3, y, 3, SSD1306_WHITE);
  }

  display.display();
}

// ================== Setup & Loop ==================
void setup() {
  Serial.begin(9600);
  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED tidak ditemukan"));
    while (true) {}
  }

  // Splash singkat
  renderSplash();
  delay(SPLASH_MS);

  lastMillis = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastMillis >= 1000) { // refresh ~1 detik
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
    snprintf(msg, sizeof(msg), "%s", vbuf); // salin aman
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
