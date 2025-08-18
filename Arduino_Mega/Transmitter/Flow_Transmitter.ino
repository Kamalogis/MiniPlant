// === Import & Define ===
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define SLAVE_ADDR    0x11
#define SPLASH_MS     200        // durasi splash di setup (ms)

#define FLOW_PIN      2           // INT0 (D2)
#define FLOW_K        7.5f        // YF-S201: L/min = Hz / 7.5
#define FLOW_OFFSET   0.60f       // offset kalibrasi opsional

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Flow & I2C ---
volatile unsigned long pulsa_sensor = 0;  // dihitung di ISR
unsigned long lastMillis = 0;
char msg[32] = "boot";

float debitLpm   = 0.0f;   // L/menit
float totalLiter = 0.0f;   // akumulasi liter

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

// === Helpers splash ===
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

void renderMain(float lpm, float liters, float hz, unsigned long pulsa) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Flow Transmitter"));

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(lpm, 2);
  display.println(F(" L/menit"));

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(F("Total : "));
  display.print(liters, 3);
  display.println(F(" L"));

  display.setCursor(0, 54);
  display.print(F("Pulsa/s: "));
  display.print((int)(hz + 0.5f));   // approx Hz sebagai integer
  display.print(F(" ("));
  display.print(pulsa);
  display.print(F(")"));

  display.display();
}

// === Setup ===
void setup() {
  Serial.begin(9600);

  Wire.begin(SLAVE_ADDR);
  Wire.onRequest(onRequest);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), cacahPulsa, RISING);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // gagal init OLED
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Splash singkat di setup
  renderSplash();
  delay(SPLASH_MS);

  lastMillis = millis();
}

// === Loop ===
void loop() {
  unsigned long now = millis();
  unsigned long dt_ms = now - lastMillis;

  if (dt_ms >= 1000) {                 // update layar ~tiap 1 detik
    lastMillis = now;

    // Snapshot pulsa & reset
    noInterrupts();
    unsigned long pulsa = pulsa_sensor;
    pulsa_sensor = 0;
    interrupts();

    // Hitung frekuensi (Hz) dari jumlah pulsa & delta waktu aktual
    float hz = 0.0f;
    if (dt_ms > 0) hz = (pulsa * 1000.0f) / (float)dt_ms;

    // L/menit dari Hz (dengan offset kalibrasi opsional)
    debitLpm = (hz / FLOW_K) + FLOW_OFFSET;

    // Tambah akumulasi liter sesuai interval waktu
    totalLiter += debitLpm * (dt_ms / 60000.0f);

    // Siapkan payload I2C (kirim LPM sebagai string)
    char vbuf[12];
    dtostrf(debitLpm, 1, 3, vbuf);
    noInterrupts();
    snprintf(msg, sizeof(msg), "%s", vbuf);
    interrupts();

    // OLED
    renderMain(debitLpm, totalLiter, hz, pulsa);

    // Debug
    Serial.print(F("Hz: ")); Serial.print(hz, 2);
    Serial.print(F(" | LPM: ")); Serial.print(debitLpm, 3);
    Serial.print(F(" | Total L: ")); Serial.println(totalLiter, 3);
  }
}
