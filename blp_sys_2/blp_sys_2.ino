#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <math.h>

// pins — keep these wired as-is
#define RAIN_PIN      A0
#define SOIL_PIN      A1
#define GREEN_LED     8
#define RED_LED       9
#define TFT_CS        10
#define TFT_DC        7
#define TFT_RST       6

// sensor thresholds — tune to your sensors if readings feel off
#define RAIN_THRESHOLD  100
#define SOIL_DRY        600
#define SOIL_WET        300

// the screen is exactly this big, don't ask why 128 and 160
#define SCREEN_W 128
#define SCREEN_H 160

// all RGB565, hand-picked for contrast on actual hardware (looks different than simulator)
#define D_BG          0x0841   // near-black with a hint of blue, not pure black (banding)
#define D_CARD        0x10A2   // slightly lifted surface, readable against bg
#define D_HEADER      0x0526   // deep teal-navy for the top bar
#define D_ACCENT      0x07FF   // cyan — the one "alive" color
#define D_SUBTEXT     0x7BEF   // mid-grey, for labels that shouldn't fight the data
#define D_BORDER      0x2965   // subtle edge, just enough to see the card boundary
#define D_DIVIDER     0x18C3   // horizontal rule color, darker than border

// these never change — color IS the meaning
#define S_DRY         0xFC40   // amber-orange: needs attention
#define S_WET         0x035F   // deep blue: saturated soil
#define S_OPTIMAL     0x07E0   // pure green: all good
#define S_RAIN        0x4C9F   // blue-violet: active rain
#define S_CLEAR       0xFE60   // warm yellow: sunny clear

// icon dot radius — 3px feels right at 128px wide
#define DOT_R 3

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// track previous state so we only redraw what changed
int  prevSoilValue = -1;
bool prevRaining   = false;
bool firstDraw     = true;



// fills a card with a 1px border on all sides
void drawCard(int x, int y, int w, int h, uint16_t fill) {
  tft.fillRect(x, y, w, h, fill);
  tft.drawRect(x, y, w, h, D_BORDER);
}

// thin horizontal rule — use between sections inside a card
void drawDivider(int x, int y, int w) {
  tft.drawFastHLine(x, y, w, D_DIVIDER);
}

// segmented moisture bar — gaps every 10px so it reads like a discrete scale, not a blob
// outer frame uses accent to anchor it visually
void drawMoistureBar(int x, int y, int w, int h, float pct, uint16_t fillColor) {
  // clear zone first — prevents ghost pixels from previous value
  tft.fillRect(x - 1, y - 1, w + 2, h + 2, D_CARD);

  // background track
  tft.fillRect(x, y, w, h, D_BG);

  // filled portion, gap every 10px for segmented look
  int filled = (int)(pct * (float)w);
  for (int i = 0; i < filled; i++) {
    if ((i % 10) != 9) tft.drawFastVLine(x + i, y + 1, h - 2, fillColor);
  }

  // outer frame using accent — makes the bar feel contained
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, D_ACCENT);
}

// 4-drop rain icon, horizontally centered at cx
void drawRainIcon(int cx, int y, uint16_t color) {
  int offsets[] = {-21, -7, 7, 21};
  for (int i = 0; i < 4; i++) {
    int dx = cx + offsets[i];
    int dy = y + 5;
    tft.fillCircle(dx, dy + 4, DOT_R - 1, color);
    tft.drawLine(dx - 1, dy, dx, dy + 2, color);
    tft.drawLine(dx + 1, dy, dx, dy + 2, color);
  }
}

// sun: filled circle core + 8 ray lines
// ray length fixed at 4px — any longer and it clips on tight layouts
void drawSunIcon(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy, 4, color);
  for (int i = 0; i < 8; i++) {
    float a = i * 3.14159f / 4.0f;
    int x1 = cx + (int)(6  * cosf(a));
    int y1 = cy + (int)(6  * sinf(a));
    int x2 = cx + (int)(10 * cosf(a));
    int y2 = cy + (int)(10 * sinf(a));
    tft.drawLine(x1, y1, x2, y2, color);
  }
}


// maps raw soil ADC to a human label + status color
// raw: 0 = fully wet, 1023 = bone dry (most capacitive sensors work this way)
void getSoilStatus(int val, char* label, uint16_t &color) {
  if (val > SOIL_DRY) {
    strcpy(label, "DRY");
    color = S_DRY;
  } else if (val < SOIL_WET) {
    strcpy(label, "SATURATED");
    color = S_WET;
  } else {
    strcpy(label, "OPTIMAL");
    color = S_OPTIMAL;
  }
}



// draws everything that doesn't change between readings
// call once on boot, then only redraw dynamic regions in updateDisplay()
void drawShell() {
  tft.fillScreen(D_BG);

  // ── header bar ──
  // 24px tall — just enough for 1x text with 6px padding top and bottom
  tft.fillRect(0, 0, SCREEN_W, 24, D_HEADER);

  // accent stripe under header — 2px is all it takes to feel intentional
  tft.fillRect(0, 22, SCREEN_W, 2, D_ACCENT);

  // header label, vertically centered in the 24px bar
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(8, 8);
  tft.print("GARDEN MONITOR");

  // live indicator dot — top right, pulses in updateDisplay() each cycle
  tft.drawCircle(116, 12, 3, D_ACCENT);

  // ── soil card ──
  // starts at y=28 (24px header + 2px accent + 2px gap)
  // 66px tall — fits status text (16px), bar (10px), labels (8px) with breathing room
  drawCard(4, 28, 120, 66, D_CARD);

  tft.setTextSize(1);
  tft.setTextColor(D_ACCENT);
  tft.setCursor(8, 33);
  tft.print("SOIL MOISTURE");

  // faint divider under the section header
  drawDivider(8, 43, 112);

  // ── rain card ──
  // starts at y=100 — 6px gap between cards
  drawCard(4, 100, 120, 56, D_CARD);

  tft.setTextColor(D_ACCENT);
  tft.setCursor(8, 105);
  tft.print("RAIN STATUS");

  drawDivider(8, 115, 112);

  // ── footer ──
  // just two pixels at the very bottom — holds the screen closed
  tft.fillRect(0, 158, SCREEN_W, 2, D_ACCENT);
}



// called every loop — only repaints regions where data actually changed
// prevents the screen from flickering on every tick
void updateDisplay(int soilValue, bool raining) {

  // ── live indicator pulse ──
  // toggles the dot between filled and empty each call
  // at 1s loop delay this gives a slow heartbeat-like blink
  static bool dotFilled = false;
  dotFilled = !dotFilled;
  if (dotFilled) {
    tft.fillCircle(116, 12, 3, D_ACCENT);
  } else {
    tft.fillCircle(116, 12, 3, D_HEADER);
    tft.drawCircle(116, 12, 3, D_ACCENT);
  }

  if (soilValue != prevSoilValue || firstDraw) {
    char label[12];
    uint16_t statusColor;
    getSoilStatus(soilValue, label, statusColor);

    // clear the dynamic zone inside the card — leave 1px border intact
    tft.fillRect(5, 44, 118, 48, D_CARD);

    // large status label — primary thing you read at a glance (2x = 12px tall chars)
    tft.setTextSize(2);
    tft.setTextColor(statusColor);
    tft.setCursor(8, 47);
    tft.print(label);

    // moisture percentage — top right of the zone
    // converts raw ADC: 0=wet→100%, 1023=dry→0%
    int pct = (int)((1.0f - (float)soilValue / 1023.0f) * 100.0f);
    tft.setTextSize(1);
    tft.setTextColor(D_SUBTEXT);

    // right-align: 3 digits max ("100") + "%" = 4 glyphs × 6px = 24px
    int pctX = (pct == 100) ? 98 : (pct >= 10 ? 104 : 110);
    tft.setCursor(pctX, 47);
    tft.print(pct);
    tft.print("%");

    // moisture bar — 10px tall, spans almost full card width
    float pctF = 1.0f - ((float)soilValue / 1023.0f);
    drawMoistureBar(8, 63, 112, 10, pctF, statusColor);

    // wet / dry end labels under the bar
    tft.setTextSize(1);
    tft.setTextColor(D_SUBTEXT);
    tft.setCursor(8, 76);
    tft.print("WET");
    tft.setCursor(104, 76);
    tft.print("DRY");

    // raw ADC value — tiny, in the header row after section title
    // clear first to avoid digit-ghosts (e.g. "1023" → "200" leaves a stray "3")
    tft.fillRect(75, 32, 48, 9, D_CARD);
    tft.setTextColor(D_SUBTEXT);
    tft.setCursor(77, 33);
    tft.print("adc:");
    tft.print(soilValue);
  }

  if (raining != prevRaining || firstDraw) {
    tft.fillRect(5, 116, 118, 38, D_CARD);

    if (raining) {
      tft.setTextSize(2);
      tft.setTextColor(S_RAIN);
      tft.setCursor(8, 119);
      tft.print("RAINING");

      // 4 drops centered in the right half of the card
      drawRainIcon(88, 119, S_RAIN);

      tft.setTextSize(1);
      tft.setTextColor(D_SUBTEXT);
      tft.setCursor(8, 138);
      tft.print("sensor active");

    } else {
      tft.setTextSize(2);
      tft.setTextColor(S_CLEAR);
      tft.setCursor(8, 119);
      tft.print("NO RAIN");

      // sun icon on the right — cy=136 puts center 17px below text baseline
      drawSunIcon(104, 136, S_CLEAR);

      tft.setTextSize(1);
      tft.setTextColor(D_SUBTEXT);
      tft.setCursor(8, 138);
      tft.print("clear skies");
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

  // hardware reset before init — skipping this causes ghost init on some boards
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(D_BG);

  // show something while sensors stabilize — first few ADC reads can be garbage

  // header same as main UI so transition feels seamless
  tft.fillRect(0, 0, SCREEN_W, 24, D_HEADER);
  tft.fillRect(0, 22, SCREEN_W, 2, D_ACCENT);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(8, 8);
  tft.print("GARDEN MONITOR");

  // big centered boot label
  tft.setTextSize(2);
  tft.setTextColor(D_ACCENT);
  tft.setCursor(20, 75);
  tft.print("STARTING");

  tft.setTextSize(1);
  tft.setTextColor(D_SUBTEXT);
  tft.setCursor(30, 105);
  tft.print("initializing sensors");

  // three dots animate left-to-right — gives a sense of progress
  // each dot takes 350ms, total ~1.05s, long enough to feel deliberate
  for (int d = 0; d < 3; d++) {
    tft.fillCircle(50 + d * 14, 120, DOT_R, D_ACCENT);
    delay(350);
  }

  // short pause so user can see all three dots before it clears
  delay(400);

  drawShell();
}



void loop() {
  int rainValue = analogRead(RAIN_PIN);   // 0 = soaked, 1023 = totally dry
  int soilValue = analogRead(SOIL_PIN);   // 0 = saturated, 1023 = bone dry

  Serial.print("Rain: ");  Serial.print(rainValue);
  Serial.print(" | Soil: "); Serial.println(soilValue);

  // rain sensor reads LOW when wet — below threshold means raining
  bool raining = (rainValue < RAIN_THRESHOLD);

  // green LED on when soil is too dry — visual alarm without looking at screen
  digitalWrite(GREEN_LED, (soilValue > SOIL_DRY) ? HIGH : LOW);

  // red LED flashes during rain
  // the 200ms on/off blink stays within the 1s loop since updateDisplay has no delays
  if (raining) {
    digitalWrite(RED_LED, HIGH);
    delay(200);
    digitalWrite(RED_LED, LOW);
    delay(200);
  } else {
    digitalWrite(RED_LED, LOW);
  }

  updateDisplay(soilValue, raining);

  // 1s refresh — slow enough to avoid flicker, fast enough to catch rain onset
  delay(1000);
}