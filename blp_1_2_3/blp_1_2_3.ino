#include <math.h>

// Pin Definitions
const int TRIG_PIN = 11;
const int ECHO_PIN = 10;
const int BUZZER_PIN = 12;
const int TILT_PIN = 2;
const int THERMISTOR_PIN = A0;

// RGB LED pins (common cathode assumed)
const int RGB_RED   = 4;
const int RGB_GREEN = 6;
const int RGB_BLUE  = 5;

// Thermistor config (NTC 10K)
const float SERIES_RESISTOR = 10000.0;  // 10K ohm resistor in voltage divider
const float NOMINAL_RESISTANCE = 10000.0; // Thermistor resistance at 25°C
const float NOMINAL_TEMP = 25.0;          // Reference temp in Celsius
const float B_COEFFICIENT = 3950.0;       // Beta coefficient (check your thermistor datasheet)
const int   ADC_MAX = 1023;

// Thresholds
const float TEMP_THRESHOLD_C  = 27.0;
const int   DIST_THRESHOLD_CM = 50;

// Tone frequencies
const int BEEP_FREQ_DIST = 1000;  // Hz for proximity alert
const int BEEP_FREQ_TILT = 1500;  // Hz for tilt alert
const int BEEP_FREQ_TEMP = 2000;  // Hz for temperature alert

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TILT_PIN, INPUT_PULLUP);  // Tilt switch to GND when tilted

  pinMode(RGB_RED,   OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE,  OUTPUT);

  setRGB(0, 0, 0); // LED off
  Serial.begin(9600);
}

void loop() {
  float tempC    = readThermistorTemp();
  long  distCm   = readDistance();
  bool  tilted   = (digitalRead(TILT_PIN) == LOW); // LOW = tilt switch closed

  Serial.print("Temp: "); Serial.print(tempC);
  Serial.print(" C | Dist: "); Serial.print(distCm);
  Serial.print(" cm | Tilted: "); Serial.println(tilted ? "YES" : "NO");

  bool buzzerActive = false;
  int  buzzerFreq   = 0;

  // --- Priority: Temperature (highest) > Tilt > Distance ---

  if (tempC > TEMP_THRESHOLD_C) {
    setRGB(255, 0, 0);       // Red LED for high temp
    buzzerFreq   = BEEP_FREQ_TEMP;
    buzzerActive = true;
  } else {
    setRGB(0, 0, 0);         // LED off if temp is fine
  }

  if (tilted) {
    buzzerFreq   = BEEP_FREQ_TILT;
    buzzerActive = true;
  }

  if (distCm > 0 && distCm < DIST_THRESHOLD_CM) {
    if (!buzzerActive) buzzerFreq = BEEP_FREQ_DIST;
    buzzerActive = true;
  }

  // Beep pattern
  if (buzzerActive) {
    tone(BUZZER_PIN, buzzerFreq, 150);
    delay(300);
    noTone(BUZZER_PIN);
  } else {
    noTone(BUZZER_PIN);
  }

  delay(200);
}

// ---- Helper Functions ----

float readThermistorTemp() {
  int   raw     = analogRead(THERMISTOR_PIN);
  float voltage = (float)raw / ADC_MAX;

  // Steinhart-Hart simplified (Beta equation)
  float resistance = SERIES_RESISTOR * (1.0 / voltage - 1.0);

  float steinhart;
  steinhart  = resistance / NOMINAL_RESISTANCE;      // R/R0
  steinhart  = log(steinhart);                        // ln(R/R0)
  steinhart /= B_COEFFICIENT;                         // / B
  steinhart += 1.0 / (NOMINAL_TEMP + 273.15);         // + 1/T0
  steinhart  = 1.0 / steinhart;                       // invert
  float tempC = steinhart - 273.15;                   // convert K → C

  return tempC;
}

long readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999;                   // out of range

  long distCm = duration * 0.034 / 2;
  return distCm;
}

void setRGB(int r, int g, int b) {
  analogWrite(RGB_RED,   r);
  analogWrite(RGB_GREEN, g);
  analogWrite(RGB_BLUE,  b);
}