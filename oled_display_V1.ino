#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include "esp_sleep.h"


#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define I2C_SDA       5
#define I2C_SCL       6
#define TOUCH_PIN     1   // Harus GPIO 0-5 untuk wake-up deep sleep

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── State ──────────────────────────────────────────────────
enum State { STATE_BIRTHDAY, STATE_EYE, STATE_OFF };
State currentState = STATE_BIRTHDAY;

// ── Touch (TTP223 MODE TOGGLE) ─────────────────────────────
bool     lastTouch        = false;
bool     longPressHandled = false;
bool     dbStable         = false;
bool     dbCandidate      = false;
uint32_t dbLastChange     = 0;
uint32_t highStartMs      = 0;
const uint32_t DEBOUNCE_MS   = 30;
const uint32_t LONG_PRESS_MS = 2000;

// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED gagal!"));
    for (;;);
  }

  // Cek apakah baru bangun dari deep sleep
  esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
  if (wakeup == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("Bangun dari deep sleep!");
  } else {
    Serial.println("Power on normal");
  }

  display.clearDisplay();
  display.display();
}

// ══════════════════════════════════════════════════════════
void loop() {
  handleTouch();

  switch (currentState) {
    case STATE_BIRTHDAY: runBirthday(); break;
    case STATE_EYE:      runEye();      break;
    case STATE_OFF:      break;
  }
}

// ══════════════════════════════════════════════════════════
//  DEEP SLEEP
// ══════════════════════════════════════════════════════════
void goToSleep() {
  Serial.println("Menunggu jari dilepas...");

  // Tunggu sampai sensor benar-benar LOW (jari sudah diangkat)
  // supaya tidak langsung wake-up lagi
  while (digitalRead(TOUCH_PIN) == HIGH) {
    delay(20);
  }
  delay(200); // beri jeda tambahan biar benar-benar stabil

  Serial.println("Masuk deep sleep...");

  // Matikan OLED dulu
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  delay(100);

  // Wake-up saat TOUCH_PIN jadi HIGH (sentuh sensor)
  // TTP223 mode toggle: sentuh → output HIGH → bangunkan ESP32
  esp_deep_sleep_enable_gpio_wakeup(1ULL << TOUCH_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);

  esp_deep_sleep_start();
  // Setelah ini ESP32 restart dari setup() saat disentuh
}

// ══════════════════════════════════════════════════════════
//  TOUCH HANDLER
// ══════════════════════════════════════════════════════════
void handleTouch() {
  bool raw = digitalRead(TOUCH_PIN);
  uint32_t now = millis();

  // Debounce 30ms
  if (raw != dbCandidate) {
    dbCandidate  = raw;
    dbLastChange = now;
  }
  if ((now - dbLastChange) >= DEBOUNCE_MS) {
    dbStable = dbCandidate;
  }

  bool stateChanged = (dbStable != lastTouch);

  // Transisi LOW→HIGH: mulai hitung durasi sentuh
  if (stateChanged && dbStable == true) {
    highStartMs      = now;
    longPressHandled = false;
  }

  // Selama HIGH: cek long press → deep sleep
  if (dbStable == true && !longPressHandled) {
    if ((now - highStartMs) >= LONG_PRESS_MS) {
      longPressHandled = true;
      goToSleep(); // ESP32 tidur di sini
    }
  }

  // Transisi HIGH→LOW: TAP → ganti display
  if (stateChanged && dbStable == false) {
    if (!longPressHandled) {
      if (currentState == STATE_BIRTHDAY) {
        currentState = STATE_EYE;
        Serial.println("TAP → Eye");
      } else if (currentState == STATE_EYE) {
        currentState = STATE_BIRTHDAY;
        Serial.println("TAP → Birthday");
      }
    }
    longPressHandled = false;
  }

  lastTouch = dbStable;
}

// ══════════════════════════════════════════════════════════
//  DISPLAY 1 – BIRTHDAY
// ══════════════════════════════════════════════════════════
void runBirthday() {
  if (currentState != STATE_BIRTHDAY) return;
  animasiHBD();
  if (currentState != STATE_BIRTHDAY) return;

  animasiKembangApi(64, 30);
  delay(300);
  if (currentState != STATE_BIRTHDAY) return;

  animasiKembangApi(30, 20);
  delay(300);
  if (currentState != STATE_BIRTHDAY) return;

  animasiKembangApi(98, 25);
  delay(500);
  if (currentState != STATE_BIRTHDAY) return;

  animasiBungaMekar();

  uint32_t holdStart = millis();
  while (millis() - holdStart < 3000) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    delay(20);
  }
}

// ══════════════════════════════════════════════════════════
//  DISPLAY 2 – MATA
// ══════════════════════════════════════════════════════════
void runEye() {
  uint32_t startMs = millis();

  while (currentState == STATE_EYE) {
    handleTouch();
    if (currentState != STATE_EYE) break;

    uint32_t t = millis() - startMs;
    display.clearDisplay();

    // Pupil gerak sinusoidal pelan dan lembut
    float px = sin((float)t / 900.0f) * 4.0f;
    float py = sin((float)t / 1300.0f) * 2.0f;

    // Kedip setiap 3 detik, durasi 180ms
    bool blinking = ((t % 3000) < 180);

    // Ekspresi senang sesekali (setelah kedip, 0.5 detik)
    bool happy = ((t % 3000) > 180 && (t % 3000) < 600);

    // Posisi mata chibi: agak lebih ke tengah layar
    drawChibiEye(34, 32, (int8_t)px, (int8_t)py, blinking, happy);
    drawChibiEye(94, 32, (int8_t)px, (int8_t)py, blinking, happy);

    // Ekspresi mulut kecil lucu di bawah
    if (!blinking) {
      if (happy) {
        // Senyum
        display.drawFastHLine(56, 52, 16, SSD1306_WHITE);
        display.drawPixel(55, 51, SSD1306_WHITE);
        display.drawPixel(72, 51, SSD1306_WHITE);
      } else {
        // Mulut netral lucu: titik-titik kecil
        display.fillCircle(60, 52, 1, SSD1306_WHITE);
        display.fillCircle(64, 53, 1, SSD1306_WHITE);
        display.fillCircle(68, 52, 1, SSD1306_WHITE);
      }
    }

    // Pipi blush kecil (tanda cute)
    if (!blinking) {
      display.drawFastHLine(10, 44, 8, SSD1306_WHITE);
      display.drawFastHLine(11, 45, 6, SSD1306_WHITE);
      display.drawFastHLine(110, 44, 8, SSD1306_WHITE);
      display.drawFastHLine(111, 45, 6, SSD1306_WHITE);
    }

    display.display();
    delay(16);
  }
}

// ── Mata Chibi ─────────────────────────────────────────────
#define EYE_R    14  // radius mata bulat besar
#define IRIS_R    9  // radius iris
#define PUPIL_R   5  // radius pupil hitam
#define SHINE_R   2  // kilap putih

void drawChibiEye(int16_t cx, int16_t cy, int8_t dx, int8_t dy, bool blinking, bool happy) {
  if (blinking) {
    // Mata tutup: lengkung UWU ^_^
    display.drawFastHLine(cx - EYE_R, cy, EYE_R * 2, SSD1306_WHITE);
    display.drawFastHLine(cx - EYE_R + 2, cy - 1, EYE_R * 2 - 4, SSD1306_WHITE);
    return;
  }

  if (happy) {
    // Ekspresi senang: mata bulan sabit (>‿<)
    for (int x = -EYE_R; x <= EYE_R; x++) {
      int yArc = cy - (int)(sqrt(max(0, EYE_R*EYE_R - x*x)) * 0.6);
      display.drawPixel(cx + x, yArc, SSD1306_WHITE);
      display.drawPixel(cx + x, yArc + 1, SSD1306_WHITE);
    }
    return;
  }

  // Mata chibi normal: lingkaran besar + iris + pupil + kilap
  // Background hitam di dalam (bola mata)
  display.fillCircle(cx, cy, EYE_R, SSD1306_BLACK);
  // Outline mata
  display.drawCircle(cx, cy, EYE_R,     SSD1306_WHITE);
  display.drawCircle(cx, cy, EYE_R - 1, SSD1306_WHITE);

  // Iris (putih)
  int16_t ppx = constrain((int16_t)(cx + dx), cx - EYE_R + IRIS_R, cx + EYE_R - IRIS_R);
  int16_t ppy = constrain((int16_t)(cy + dy), cy - EYE_R + IRIS_R, cy + EYE_R - IRIS_R);
  display.fillCircle(ppx, ppy, IRIS_R, SSD1306_WHITE);

  // Pupil hitam
  display.fillCircle(ppx, ppy, PUPIL_R, SSD1306_BLACK);

  // Kilap (shine) kanan atas pupil
  display.fillCircle(ppx + 3, ppy - 3, SHINE_R, SSD1306_WHITE);
  // Kilap kecil kedua
  display.fillCircle(ppx - 2, ppy - 4, 1, SSD1306_WHITE);
}

// ══════════════════════════════════════════════════════════
//  ANIMASI BIRTHDAY
// ══════════════════════════════════════════════════════════
// Gambar bunga pixel kecil (pengganti emoji 🌼)
void drawPixelFlower(int x, int y) {
  // Kelopak 4 arah
  display.drawPixel(x,   y-2, SSD1306_WHITE);
  display.drawPixel(x,   y+2, SSD1306_WHITE);
  display.drawPixel(x-2, y,   SSD1306_WHITE);
  display.drawPixel(x+2, y,   SSD1306_WHITE);
  // Diagonal
  display.drawPixel(x-1, y-1, SSD1306_WHITE);
  display.drawPixel(x+1, y-1, SSD1306_WHITE);
  display.drawPixel(x-1, y+1, SSD1306_WHITE);
  display.drawPixel(x+1, y+1, SSD1306_WHITE);
  // Inti
  display.drawPixel(x, y, SSD1306_WHITE);
}

void animasiHBD() {
  // ── FASE 1: Happy Birthday + nama jatuh dari atas ──────
  int textY = -40;
  while (textY < 10) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    display.clearDisplay();

    // Bintang background
    for (int i = 0; i < 8; i++)
      display.drawPixel(rand() % 128, rand() % 64, SSD1306_WHITE);

    display.setTextColor(SSD1306_WHITE);

    // "Happy Birthday" size 1 biar muat dengan nama
    display.setTextSize(1);
    display.setCursor(22, textY);
    display.print("* Happy Birthday *");

    // Nama size 2 lebih besar & menonjol
    display.setTextSize(2);
    display.setCursor(20, textY + 14);
    display.print("Nama kalian");

    // Bunga pixel di kanan nama
    if (textY + 20 < 64 && textY + 20 >= 0)
      drawPixelFlower(105, textY + 22);

    display.display();
    textY += 2;
    delay(18);
  }

  // ── FASE 2: Teks bergetar + bingkai ────────────────────
  for (int i = 0; i < 25; i++) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    display.clearDisplay();

    int offset = (i % 2 == 0) ? 1 : 0;

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(22 + offset, 10);
    display.print("* Happy Birthday *");

    display.setTextSize(2);
    display.setCursor(20 - offset, 24);
    display.print("Nama kalian");

    // Bunga pixel
    drawPixelFlower(105, 32);
    drawPixelFlower(8,   32);

    // Bingkai berkedip
    display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    if (i % 2 == 0)
      display.drawRect(4, 4, 120, 56, SSD1306_WHITE);

    display.display();
    delay(100);
  }

  // ── FASE 3: Pesan "Keep growing..." muncul per kata ────
  // Tampilkan 3 detik dengan efek muncul bertahap
  const char* line1 = "Keep growing,";
  const char* line2 = "the world";
  const char* line3 = "needs your light.";

  // Hitung total karakter untuk timing
  int totalChars = strlen(line1) + strlen(line2) + strlen(line3);

  for (int frame = 0; frame <= totalChars + 20; frame++) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;

    display.clearDisplay();

    // Bintang kecil di background
    srand(42); // seed tetap supaya bintang tidak bergerak
    for (int i = 0; i < 6; i++)
      display.drawPixel(rand() % 128, rand() % 20, SSD1306_WHITE);

    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    // Line 1
    int show1 = min(frame, (int)strlen(line1));
    display.setCursor(10, 20);
    for (int c = 0; c < show1; c++) display.print(line1[c]);

    // Line 2
    int show2 = min(max(0, frame - (int)strlen(line1)), (int)strlen(line2));
    display.setCursor(22, 34);
    for (int c = 0; c < show2; c++) display.print(line2[c]);

    // Line 3
    int show3 = min(max(0, frame - (int)strlen(line1) - (int)strlen(line2)), (int)strlen(line3));
    display.setCursor(4, 48);
    for (int c = 0; c < show3; c++) display.print(line3[c]);

    // Bunga kecil di pojok saat semua teks muncul
    if (frame >= totalChars) {
      drawPixelFlower(118, 20);
      drawPixelFlower(4,   55);
    }

    display.display();
    delay(60);
  }

  // Tahan pesan 2 detik
  uint32_t holdMsg = millis();
  while (millis() - holdMsg < 2000) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    delay(20);
  }
}

void animasiKembangApi(int targetX, int targetY) {
  for (int y = 64; y > targetY; y -= 3) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    display.clearDisplay();
    display.drawPixel(targetX, y, SSD1306_WHITE);
    display.drawPixel(targetX, y + 1, SSD1306_WHITE);
    display.display();
    delay(10);
  }

  int jumlahPartikel = 24;
  float kecepatan[24], sudut[24];
  for (int i = 0; i < jumlahPartikel; i++) {
    sudut[i]     = (i * 2 * M_PI) / jumlahPartikel;
    kecepatan[i] = (rand() % 10 + 5) / 5.0;
  }

  for (int t = 0; t < 25; t++) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    display.clearDisplay();
    for (int i = 0; i < jumlahPartikel; i++) {
      int pX = targetX + cos(sudut[i]) * kecepatan[i] * t;
      int pY = targetY + sin(sudut[i]) * kecepatan[i] * t + (t * t * 0.05);
      if (pX >= 0 && pX < 128 && pY >= 0 && pY < 64) {
        if (t < 18 || t % 2 == 0)
          display.drawPixel(pX, pY, SSD1306_WHITE);
      }
    }
    display.display();
    delay(25);
  }
}

// Fungsi gambar 1 bunga dengan rose curve
void drawBunga(int cx, int cy, float maxRadius, int kelopak) {
  for (float theta = 0; theta < 2 * M_PI; theta += 0.02) {
    float r = maxRadius * sin(kelopak * theta);
    if (r < 0) r = -r;
    int x = cx + r * cos(theta);
    int y = cy + r * sin(theta);
    if (x >= 0 && x < 128 && y >= 0 && y < 64)
      display.drawPixel(x, y, SSD1306_WHITE);
  }
  // Putik
  int putik = max(2, (int)(maxRadius * 0.25));
  display.fillCircle(cx, cy, putik, SSD1306_WHITE);
}

void animasiBungaMekar() {
  // Posisi: 1 bunga besar tengah, 2 bunga kecil kiri & kanan
  int cxT = 64, cyT = 32; // tengah (besar)
  int cxL = 20, cyL = 40; // kiri (kecil)
  int cxR = 108, cyR = 40; // kanan (kecil)
  float maxBesar = 22;
  float maxKecil = 12;

  // Tangkai tumbuh dulu
  for (int h = 64; h >= cyT + 12; h--) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    display.clearDisplay();
    // Tangkai tengah
    display.drawLine(cxT, 64, cxT, h, SSD1306_WHITE);
    // Tangkai kiri & kanan (muncul lebih pendek)
    if (h < 58) {
      display.drawLine(cxL, 64, cxL, 58, SSD1306_WHITE);
      display.drawLine(cxR, 64, cxR, 58, SSD1306_WHITE);
    }
    display.display();
    delay(12);
  }

  // Bunga mekar bertahap: kecil dulu, lalu besar
  float rBesar = 0, rKecil = 0;
  while (rBesar <= maxBesar) {
    handleTouch();
    if (currentState != STATE_BIRTHDAY) return;
    display.clearDisplay();

    // Tangkai
    display.drawLine(cxT, 64, cxT, cyT + 10, SSD1306_WHITE);
    display.drawLine(cxL, 64, cxL, cyL + 6,  SSD1306_WHITE);
    display.drawLine(cxR, 64, cxR, cyR + 6,  SSD1306_WHITE);
    // Daun tengah
    display.drawTriangle(cxT, cyT+20, cxT-6, cyT+14, cxT, cyT+12, SSD1306_WHITE);
    display.drawTriangle(cxT, cyT+23, cxT+6, cyT+17, cxT, cyT+15, SSD1306_WHITE);

    // Bunga kecil kiri & kanan (5 kelopak)
    if (rKecil > 0) {
      drawBunga(cxL, cyL, rKecil, 5);
      drawBunga(cxR, cyR, rKecil, 5);
    }
    // Bunga besar tengah (6 kelopak)
    if (rBesar > 0) {
      drawBunga(cxT, cyT, rBesar, 6);
    }

    display.display();

    rKecil += 0.5;
    if (rKecil > maxKecil) rKecil = maxKecil;
    rBesar += 0.7;
    delay(18);
  }
}
