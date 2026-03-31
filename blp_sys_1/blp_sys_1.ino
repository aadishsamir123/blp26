const int THERM_PIN = A0;
const int RED_PIN   = 5;
const int GRN_PIN   = 9;
const int BLU_PIN   = 11;
const int BUZZER    = 13;

// Claude made the next few lines for using Steinhart-Hart Equation with thermistor.
// The steinhart-hart equation is needed, because the thermistor is a RESISTOR, not a temperature sensor.
// Therefore, calculations are needed to calculate the temperature based on the resistance.

const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 10000.0;
const float NOMINAL_TEMP = 25.0;
const float B_COEFFICIENT = 3950.0;

const float NORMAL_THRESHOLD  = 27.0;
const float WARNING_THRESHOLD = 30.0;

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GRN_PIN, OUTPUT);
  pinMode(BLU_PIN, OUTPUT);
  pinMode(BUZZER,  OUTPUT);
  Serial.begin(9600);
  // Fancy Shmancy Noises on startup
  tone(BUZZER, 5000);
  delay(100);
  tone(BUZZER, 0);
  delay(100);
}

float readTempC() { // yes a variable needs such a huge function
  int raw = analogRead(THERM_PIN);
  float resistance = SERIES_RESISTOR / (1023.0 / raw - 1.0);
  
  // more Steinhart-Hart equation by claude
  float steinhart = resistance / NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (NOMINAL_TEMP + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;  // convert to Celsius
  
  return steinhart;
}

void setRGB(bool red, bool green, bool blue) { // setting LED pins for RGB LED
  digitalWrite(RED_PIN, red);
  digitalWrite(GRN_PIN, green);
  digitalWrite(BLU_PIN, blue);
}

void loop() {
  float tempC = readTempC();

  Serial.print("Temp: ");
  Serial.print(tempC);
  Serial.println(" C");

  if (tempC >= WARNING_THRESHOLD) {
    // FIRE — red led, rapid high beep
    setRGB(true, false, false);
    for (int i = 0; i < 5; i++) {
      tone(BUZZER, 1000);
      delay(100);
      noTone(BUZZER);
      delay(100);
    }

  } else if (tempC >= NORMAL_THRESHOLD) {
    // WARNING — yellow led, slow medium beep
    setRGB(true, true, false);
    tone(BUZZER, 500);
    delay(500);
    noTone(BUZZER);
    delay(500);

  } else {
    // NORMAL — green led, silence
    setRGB(false, true, false);
    noTone(BUZZER);
    delay(1000);
  }
}