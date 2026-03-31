#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <math.h>

// --- Pin Definitions ---
#define RAIN_PIN      A0
#define SOIL_PIN      A1
#define GREEN_LED     8
#define RED_LED       9
#define TFT_CS        10
#define TFT_DC        7
#define TFT_RST       6

// --- Thresholds ---
#define RAIN_THRESHOLD  100
#define SOIL_DRY        600
#define SOIL_WET        300

// ST7735 are 128x160
#define SCREEN_W 128
#define SCREEN_H 160

// --- Dark mode colors ---
#define D_BG          0x0841
#define D_CARD        0x1082
#define D_HEADER      0x0438
#define D_ACCENT      0x04FF
#define D_SUBTEXT     0x8410
#define D_BORDER      0x2945

// --- Light mode colors ---
#define L_BG          0xEF7D
#define L_CARD        0xFFFF
#define L_HEADER      0x2C9F
#define L_ACCENT      0x035F
#define L_SUBTEXT     0x6B4D
#define L_BORDER      0xC618

// --- Status colors ---
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x035F
#define COLOR_ORANGE    0xFC60
#define COLOR_YELLOW    0xFFE0

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

int  prevSoilValue = -1;
bool prevRaining   = false;
bool firstDraw     = true;
bool lightMode     = false;

uint16_t C(uint16_t dark, uint16_t light) {
  return lightMode ? light : dark;
}

// --- Draw rounded card ---
void drawCard(int x, int y, int w, int h, uint16_t color) {
  tft.fillRect(x + 2, y,     w - 4, h,     color);
  tft.fillRect(x,     y + 2, w,     h - 4, color);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, C(D_BORDER, L_BORDER));
}

// --- Draw segmented moisture bar ---
void drawBar(int x, int y, int w, int h, float pct, uint16_t fillColor) {
  tft.fillRect(x, y, w, h, C(D_BORDER, L_BORDER));
  int filled = (int)(pct * w);
  for (int i = 0; i < filled; i++) {
    if (i % 10 != 9) tft.drawFastVLine(x + i, y + 1, h - 2, fillColor);
  }
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, C(D_ACCENT, L_ACCENT));
}

// --- Draw raindrop ---
void drawRaindrop(int x, int y, uint16_t color) {
  tft.fillCircle(x, y + 3, 2, color);
  tft.drawLine(x, y, x - 1, y + 2, color);
  tft.drawLine(x, y, x + 1, y + 2, color);
}

// --- Draw sun ---
void drawSun(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy, 5, color);
  for (int a = 0; a < 8; a++) {
    float angle = a * 3.14159f / 4.0f;
    int x1 = cx + (int)(7  * cos(angle));
    int y1 = cy + (int)(7  * sin(angle));
    int x2 = cx + (int)(11 * cos(angle));
    int y2 = cy + (int)(11 * sin(angle));
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

// --- Soil label + color ---
void getSoilStatus(int val, char* label, uint16_t &color) {
  if (val > SOIL_DRY) {
    strcpy(label, "DRY");
    color = COLOR_ORANGE;
  } else if (val < SOIL_WET) {
    strcpy(label, "VERY WET");
    color = COLOR_BLUE;
  } else {
    strcpy(label, "OPTIMAL");
    color = COLOR_GREEN;
  }
}

// --- Draw static shell ---
void drawShell() {
  tft.fillScreen(C(D_BG, L_BG));

  // Header
  tft.fillRect(0, 0, SCREEN_W, 22, C(D_HEADER, L_HEADER));
  tft.fillRect(0, 19, SCREEN_W, 3, C(D_ACCENT, L_ACCENT));
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(10, 8);
  tft.print("GARDEN MONITOR");

  // Soil card
  drawCard(4, 26, 120, 62, C(D_CARD, L_CARD));
  tft.setTextSize(1);
  tft.setTextColor(C(D_ACCENT, L_ACCENT));
  tft.setCursor(10, 31);
  tft.print("SOIL");

  // Rain card
  drawCard(4, 94, 120, 62, C(D_CARD, L_CARD));
  tft.setCursor(10, 99);
  tft.print("RAIN STATUS");

  // Footer
  tft.fillRect(0, 157, SCREEN_W, 3, C(D_ACCENT, L_ACCENT));
}

void updateDisplay(int soilValue, bool raining) {

  // ── SOIL CARD ──
  if (soilValue != prevSoilValue || firstDraw) {
    char label[10];
    uint16_t statusColor;
    getSoilStatus(soilValue, label, statusColor);

    tft.fillRect(6, 40, 116, 44, C(D_CARD, L_CARD));

    // Big status label
    tft.setTextSize(2);
    tft.setTextColor(statusColor);
    tft.setCursor(10, 42);
    tft.print(label);

    // Pct top right
    int pctVal = (int)((1.0f - soilValue / 1023.0f) * 100);
    tft.setTextSize(1);
    tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT));
    tft.setCursor(92, 42);
    tft.print(pctVal);
    tft.print("%");

    // Segmented bar
    float pct = 1.0f - (soilValue / 1023.0f);
    drawBar(10, 60, 108, 8, pct, statusColor);

    // Wet/Dry labels under bar
    tft.setTextSize(1);
    tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT));
    tft.setCursor(10, 71);
    tft.print("WET");
    tft.setCursor(100, 71);
    tft.print("DRY");

    // Raw value
    tft.fillRect(54, 29, 46, 9, C(D_CARD, L_CARD));
    tft.setCursor(56, 30);
    tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT));
    tft.print("raw:");
    tft.print(soilValue);
  }

  // ── RAIN CARD ──
  if (raining != prevRaining || firstDraw) {
    tft.fillRect(6, 108, 116, 46, C(D_CARD, L_CARD));

    if (raining) {
      tft.setTextSize(2);
      tft.setTextColor(COLOR_BLUE);
      tft.setCursor(10, 112);
      tft.print("RAINING!");

      // Three raindrops
      drawRaindrop(24, 132, COLOR_BLUE);
      drawRaindrop(56, 132, COLOR_BLUE);
      drawRaindrop(88, 132, COLOR_BLUE);

    } else {
      tft.setTextSize(2);
      tft.setTextColor(lightMode ? 0xFCC0 : COLOR_YELLOW);
      tft.setCursor(10, 112);
      tft.print("NO RAIN");

      // Sun
      drawSun(104, 136, lightMode ? 0xFCC0 : COLOR_YELLOW);

      tft.setTextSize(1);
      tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT));
      tft.setCursor(10, 134);
      tft.print("Clear skies");
    }
  }

  prevSoilValue = soilValue;
  prevRaining   = raining;
  firstDraw     = false;
}

void setup() {
  Serial.begin(9600);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Force hardware reset before init
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(C(D_BG, L_BG));

  // Loading screen
  tft.fillRect(0, 0, SCREEN_W, 22, C(D_HEADER, L_HEADER));
  tft.fillRect(0, 19, SCREEN_W, 3, C(D_ACCENT, L_ACCENT));
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(10, 8);
  tft.print("GARDEN MONITOR");

  tft.setTextSize(2);
  tft.setTextColor(C(D_ACCENT, L_ACCENT));
  tft.setCursor(22, 85);
  tft.print("LOADING");

  // Animated dots
  for (int d = 0; d < 3; d++) {
    tft.fillCircle(44 + d * 16, 110, 3, C(D_ACCENT, L_ACCENT));
    delay(400);
  }
  delay(300);

  drawShell();
}

void loop() {
  int rainValue = analogRead(RAIN_PIN);  // 0 is soaked, 1023 is dry
  int soilValue = analogRead(SOIL_PIN);  // 0 is wet, 1023 is dry

  Serial.print("Rain: "); Serial.print(rainValue);
  Serial.print(" | Soil: "); Serial.println(soilValue);

  bool raining = rainValue > RAIN_THRESHOLD;

  // --- Green LED lights when soil is dry ---
  digitalWrite(GREEN_LED, (soilValue > SOIL_DRY) ? HIGH : LOW);

  // --- Red LED is flashing when it raining ---
  if (raining) {
    digitalWrite(RED_LED, HIGH);
    delay(200);
    digitalWrite(RED_LED, LOW);
    delay(200);
  } else {
    digitalWrite(RED_LED, LOW);
  }

  updateDisplay(soilValue, raining);

  delay(1000); // refresh every a second
}