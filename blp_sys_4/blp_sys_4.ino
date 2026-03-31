#include <Adafruit_GFX.h>      // includes the graphics library for drawing shapes and text
#include <Adafruit_ST7735.h>   // includes the library for the ST7735 TFT display
#include <SPI.h>               // includes the SPI library for communication with the display
#include <math.h>              // includes the math library for cos/sin functions used in drawing the sun

// --- Pin Definitions ---
#define RAIN_PIN      A0       // rain sensor is connected to analog pin A0
#define SOIL_PIN      A1       // soil moisture sensor is connected to analog pin A1
#define GREEN_LED     8        // green LED is connected to digital pin 8
#define RED_LED       9        // red LED is connected to digital pin 9
#define TFT_CS        10       // TFT chip select pin is connected to digital pin 10
#define TFT_DC        7        // TFT data/command pin is connected to digital pin 7
#define TFT_RST       6        // TFT reset pin is connected to digital pin 6

// --- Thresholds ---
#define RAIN_THRESHOLD  100    // if rain sensor reads above 100 it is considered raining
#define SOIL_DRY        600    // if soil sensor reads above 600 the soil is considered dry
#define SOIL_WET        300    // if soil sensor reads below 300 the soil is considered very wet

// ST7735 are 128x160
#define SCREEN_W 128           // screen width is 128 pixels
#define SCREEN_H 160           // screen height is 160 pixels

// --- Dark mode colors ---
#define D_BG          0x0841   // dark mode background color
#define D_CARD        0x1082   // dark mode card background color
#define D_HEADER      0x0438   // dark mode header color
#define D_ACCENT      0x04FF   // dark mode accent/highlight color
#define D_SUBTEXT     0x8410   // dark mode subtext color
#define D_BORDER      0x2945   // dark mode border color

// --- Light mode colors ---
#define L_BG          0xEF7D   // light mode background color
#define L_CARD        0xFFFF   // light mode card background color (white)
#define L_HEADER      0x2C9F   // light mode header color
#define L_ACCENT      0x035F   // light mode accent/highlight color
#define L_SUBTEXT     0x6B4D   // light mode subtext color
#define L_BORDER      0xC618   // light mode border color

// --- Status colors ---
#define COLOR_GREEN     0x07E0 // green color used for optimal soil status
#define COLOR_BLUE      0x035F // blue color used for very wet soil and rain
#define COLOR_ORANGE    0xFC60 // orange color used for dry soil status
#define COLOR_YELLOW    0xFFE0 // yellow color used for no rain/sun icon

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); // creates the TFT display object using the defined pins

int  prevSoilValue = -1;   // stores the previous soil value to avoid unnecessary screen redraws
bool prevRaining   = false; // stores the previous rain state to avoid unnecessary screen redraws
bool firstDraw     = true;  // tracks if this is the first time drawing so everything draws on startup
bool lightMode     = false; // stores whether light mode is on or off, false means dark mode is default

uint16_t C(uint16_t dark, uint16_t light) { // helper function that returns dark or light color depending on current mode
  return lightMode ? light : dark;           // if lightMode is true return light color, otherwise return dark color
}

// --- Draw rounded card ---
void drawCard(int x, int y, int w, int h, uint16_t color) {
  tft.fillRect(x + 2, y,     w - 4, h,     color); // draws the main body of the card slightly inset horizontally to create rounded effect
  tft.fillRect(x,     y + 2, w,     h - 4, color); // draws the main body of the card slightly inset vertically to create rounded effect
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, C(D_BORDER, L_BORDER)); // draws a border around the card
}

// --- Draw segmented moisture bar ---
void drawBar(int x, int y, int w, int h, float pct, uint16_t fillColor) {
  tft.fillRect(x, y, w, h, C(D_BORDER, L_BORDER));  // fills the entire bar background with border color
  int filled = (int)(pct * w);                        // calculates how many pixels to fill based on the percentage
  for (int i = 0; i < filled; i++) {                 // loops through each pixel that should be filled
    if (i % 10 != 9) tft.drawFastVLine(x + i, y + 1, h - 2, fillColor); // draws a vertical line for each pixel except every 10th one creating a segmented look
  }
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, C(D_ACCENT, L_ACCENT)); // draws an accent border around the outside of the bar
}

// --- Draw raindrop ---
void drawRaindrop(int x, int y, uint16_t color) {
  tft.fillCircle(x, y + 3, 2, color);       // draws the round bottom part of the raindrop
  tft.drawLine(x, y, x - 1, y + 2, color);  // draws the left side of the pointed top of the raindrop
  tft.drawLine(x, y, x + 1, y + 2, color);  // draws the right side of the pointed top of the raindrop
}

// --- Draw sun ---
void drawSun(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy, 5, color);                  // draws the main circle of the sun
  for (int a = 0; a < 8; a++) {                      // loops 8 times to draw 8 rays around the sun
    float angle = a * 3.14159f / 4.0f;               // calculates the angle for each ray evenly spaced at 45 degrees apart
    int x1 = cx + (int)(7  * cos(angle));             // calculates the starting x position of the ray
    int y1 = cy + (int)(7  * sin(angle));             // calculates the starting y position of the ray
    int x2 = cx + (int)(11 * cos(angle));             // calculates the ending x position of the ray
    int y2 = cy + (int)(11 * sin(angle));             // calculates the ending y position of the ray
    tft.drawLine(x1, y1, x2, y2, color);             // draws the ray line from start to end position
  }
}

// --- Soil label + color ---
void getSoilStatus(int val, char* label, uint16_t &color) {
  if (val > SOIL_DRY) {           // if soil value is above 600 (dry threshold)
    strcpy(label, "DRY");         // sets the label text to DRY
    color = COLOR_ORANGE;         // sets the color to orange for dry status
  } else if (val < SOIL_WET) {   // if soil value is below 300 (wet threshold)
    strcpy(label, "VERY WET");   // sets the label text to VERY WET
    color = COLOR_BLUE;           // sets the color to blue for very wet status
  } else {                        // if soil value is between 300 and 600 (optimal range)
    strcpy(label, "OPTIMAL");    // sets the label text to OPTIMAL
    color = COLOR_GREEN;          // sets the color to green for optimal status
  }
}

// --- Draw static shell ---
void drawShell() {
  tft.fillScreen(C(D_BG, L_BG)); // fills the entire screen with the background color

  // Header
  tft.fillRect(0, 0, SCREEN_W, 22, C(D_HEADER, L_HEADER));      // draws the header bar at the top of the screen
  tft.fillRect(0, 19, SCREEN_W, 3, C(D_ACCENT, L_ACCENT));      // draws a thin accent line below the header
  tft.setTextSize(1);                                             // sets text size to 1 (smallest)
  tft.setTextColor(0xFFFF);                                       // sets text color to white
  tft.setCursor(10, 8);                                           // positions the cursor for the header text
  tft.print("GARDEN MONITOR");                                    // prints the header title

  // Soil card
  drawCard(4, 26, 120, 62, C(D_CARD, L_CARD));  // draws the soil card background
  tft.setTextSize(1);                             // sets text size to 1
  tft.setTextColor(C(D_ACCENT, L_ACCENT));        // sets text color to accent color
  tft.setCursor(10, 31);                          // positions cursor for soil label
  tft.print("SOIL");                              // prints the soil card title

  // Rain card
  drawCard(4, 94, 120, 62, C(D_CARD, L_CARD));  // draws the rain card background
  tft.setCursor(10, 99);                          // positions cursor for rain label
  tft.print("RAIN STATUS");                       // prints the rain card title

  // Footer
  tft.fillRect(0, 157, SCREEN_W, 3, C(D_ACCENT, L_ACCENT)); // draws a thin accent line at the bottom of the screen
}

void updateDisplay(int soilValue, bool raining) {

  // ── SOIL CARD ──
  if (soilValue != prevSoilValue || firstDraw) { // only redraws soil card if value changed or it is the first draw
    char label[10];         // creates a character array to store the soil status label text
    uint16_t statusColor;   // creates a variable to store the soil status color
    getSoilStatus(soilValue, label, statusColor); // calls the function to get the correct label and color based on soil value

    tft.fillRect(6, 40, 116, 44, C(D_CARD, L_CARD)); // clears the soil card content area before redrawing

    // Big status label
    tft.setTextSize(2);              // sets text size to 2 (larger)
    tft.setTextColor(statusColor);   // sets text color to the status color (orange, blue or green)
    tft.setCursor(10, 42);           // positions cursor for the status label
    tft.print(label);                // prints the status label (DRY, VERY WET or OPTIMAL)

    // Pct top right
    int pctVal = (int)((1.0f - soilValue / 1023.0f) * 100); // calculates moisture percentage (inverted because 0 is wet and 1023 is dry)
    tft.setTextSize(1);                       // sets text size back to 1
    tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT)); // sets text color to subtext color
    tft.setCursor(92, 42);                    // positions cursor for percentage display in top right of card
    tft.print(pctVal);                        // prints the moisture percentage number
    tft.print("%");                           // prints the percentage symbol

    // Segmented bar
    float pct = 1.0f - (soilValue / 1023.0f); // calculates the fill percentage for the bar (inverted)
    drawBar(10, 60, 108, 8, pct, statusColor); // draws the segmented moisture bar with the correct fill and color

    // Wet/Dry labels under bar
    tft.setTextSize(1);                        // sets text size to 1
    tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT)); // sets text color to subtext color
    tft.setCursor(10, 71);                     // positions cursor for WET label on the left
    tft.print("WET");                          // prints WET label on the left side of the bar
    tft.setCursor(100, 71);                    // positions cursor for DRY label on the right
    tft.print("DRY");                          // prints DRY label on the right side of the bar

    // Raw value
    tft.fillRect(54, 29, 46, 9, C(D_CARD, L_CARD)); // clears the area where raw value is displayed before redrawing
    tft.setCursor(56, 30);                           // positions cursor for raw value display
    tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT));       // sets text color to subtext color
    tft.print("raw:");                               // prints the raw label
    tft.print(soilValue);                            // prints the actual raw sensor value
  }

  // ── RAIN CARD ──
  if (raining != prevRaining || firstDraw) { // only redraws rain card if rain state changed or it is the first draw
    tft.fillRect(6, 108, 116, 46, C(D_CARD, L_CARD)); // clears the rain card content area before redrawing

    if (raining) {                          // if it is currently raining
      tft.setTextSize(2);                   // sets text size to 2
      tft.setTextColor(COLOR_BLUE);         // sets text color to blue for rain
      tft.setCursor(10, 112);               // positions cursor for raining text
      tft.print("RAINING!");                // prints RAINING! on the display

      // Three raindrops
      drawRaindrop(24, 132, COLOR_BLUE);    // draws first raindrop icon
      drawRaindrop(56, 132, COLOR_BLUE);    // draws second raindrop icon
      drawRaindrop(88, 132, COLOR_BLUE);    // draws third raindrop icon

    } else {                                                        // if it is not raining
      tft.setTextSize(2);                                           // sets text size to 2
      tft.setTextColor(lightMode ? 0xFCC0 : COLOR_YELLOW);         // sets text color to darker yellow in light mode or bright yellow in dark mode
      tft.setCursor(10, 112);                                       // positions cursor for no rain text
      tft.print("NO RAIN");                                         // prints NO RAIN on the display

      // Sun
      drawSun(104, 136, lightMode ? 0xFCC0 : COLOR_YELLOW);        // draws sun icon in the bottom right of the rain card

      tft.setTextSize(1);                        // sets text size to 1
      tft.setTextColor(C(D_SUBTEXT, L_SUBTEXT)); // sets text color to subtext color
      tft.setCursor(10, 134);                    // positions cursor for clear skies text
      tft.print("Clear skies");                  // prints Clear skies below the NO RAIN text
    }
  }

  prevSoilValue = soilValue; // saves current soil value as previous for next loop comparison
  prevRaining   = raining;   // saves current rain state as previous for next loop comparison
  firstDraw     = false;     // sets firstDraw to false so full redraws no longer happen every loop
}

void setup() {
  Serial.begin(9600);              // starts serial monitor at 9600 baud for debugging
  pinMode(GREEN_LED, OUTPUT);      // sets green LED pin as output
  pinMode(RED_LED, OUTPUT);        // sets red LED pin as output

  // Force hardware reset before init
  pinMode(TFT_RST, OUTPUT);        // sets TFT reset pin as output
  digitalWrite(TFT_RST, HIGH);     // sets reset pin high first
  delay(10);                       // waits 10 milliseconds
  digitalWrite(TFT_RST, LOW);      // pulls reset pin low to trigger reset
  delay(50);                       // waits 50 milliseconds for reset to complete
  digitalWrite(TFT_RST, HIGH);     // brings reset pin back high to end reset
  delay(150);                      // waits 150 milliseconds for display to fully initialize

  tft.initR(INITR_BLACKTAB);       // initializes the TFT display with black tab settings
  tft.setRotation(0);              // sets display rotation to 0 (portrait mode)
  tft.fillScreen(C(D_BG, L_BG));  // fills screen with background color

  // Loading screen
  tft.fillRect(0, 0, SCREEN_W, 22, C(D_HEADER, L_HEADER));  // draws header bar on loading screen
  tft.fillRect(0, 19, SCREEN_W, 3, C(D_ACCENT, L_ACCENT));  // draws accent line below header on loading screen
  tft.setTextSize(1);                                         // sets text size to 1
  tft.setTextColor(0xFFFF);                                   // sets text color to white
  tft.setCursor(10, 8);                                       // positions cursor for title
  tft.print("GARDEN MONITOR");                                // prints title on loading screen

  tft.setTextSize(2);                          // sets text size to 2
  tft.setTextColor(C(D_ACCENT, L_ACCENT));     // sets text color to accent color
  tft.setCursor(22, 85);                       // positions cursor for loading text
  tft.print("LOADING");                        // prints LOADING text in the middle of the screen

  // Animated dots
  for (int d = 0; d < 3; d++) {                               // loops 3 times to draw 3 animated loading dots
    tft.fillCircle(44 + d * 16, 110, 3, C(D_ACCENT, L_ACCENT)); // draws each dot spaced 16 pixels apart
    delay(400);                                                // waits 400 milliseconds between each dot appearing
  }
  delay(300);   // waits an extra 300 milliseconds after all dots appear before moving on

  drawShell();  // draws the main static UI shell after loading is complete
}

void loop() {
  int rainValue = analogRead(RAIN_PIN);  // 0 is soaked, 1023 is dry // reads the rain sensor value from analog pin A0
  int soilValue = analogRead(SOIL_PIN);  // 0 is wet, 1023 is dry    // reads the soil moisture sensor value from analog pin A1

  Serial.print("Rain: "); Serial.print(rainValue);       // prints rain sensor value to serial monitor
  Serial.print(" | Soil: "); Serial.println(soilValue);  // prints soil sensor value to serial monitor on a new line

  bool raining = rainValue > RAIN_THRESHOLD; // sets raining to true if rain sensor reads above 100 otherwise false

  // --- Green LED lights when soil is dry ---
  digitalWrite(GREEN_LED, (soilValue > SOIL_DRY) ? HIGH : LOW); // turns green LED on if soil is dry (above 600) otherwise turns it off

  // --- Red LED is flashing when it raining ---
  if (raining) {               // if it is currently raining
    digitalWrite(RED_LED, HIGH); // turns red LED on
    delay(200);                  // waits 200 milliseconds
    digitalWrite(RED_LED, LOW);  // turns red LED off
    delay(200);                  // waits 200 milliseconds creating a flashing effect
  } else {                       // if it is not raining
    digitalWrite(RED_LED, LOW);  // keeps red LED off
  }

  updateDisplay(soilValue, raining); // calls the function to update the TFT display with latest sensor readings

  delay(1000); // refresh every a second // waits 1 second before reading sensors again
}