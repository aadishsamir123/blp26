#include <math.h> // include the maths library for log() function which we needing later

// Pin Definitions
const int TRIG_PIN = 11; // trig pin of ultrasonic, it send the pulse out
const int ECHO_PIN = 10; // echo pin of ultrasonic, it listen for the bounce back
const int BUZZER_PIN = 12; // buzzer is here, it make the beep sounds
const int TILT_PIN = 2; // tilt switch pin, goes LOW when the thing is tilting
const int THERMISTOR_PIN = A0; // analog pin where thermistor is connected at

// RGB LED pins (common cathode assumed)
const int RGB_RED   = 4; // red channel of the LED, for the hot warning
const int RGB_GREEN = 6; // green channel, currently not used much but is there
const int RGB_BLUE  = 5; // blue channel, also there for future using purpose

// Thermistor config (NTC 10K)
const float SERIES_RESISTOR = 10000.0;  // the 10K resistor that sit in series with thermistor for making voltage divider
const float NOMINAL_RESISTANCE = 10000.0; // resistance value of thermistor when it at exactly 25 degree
const float NOMINAL_TEMP = 25.0;          // the reference temperature in celsius that nominal resistance is measured on
const float B_COEFFICIENT = 3950.0;       // beta value of thermistor, you should check datasheet of your specific one
const int   ADC_MAX = 1023; // maximum value that ADC can reading (10-bit so it go 0 to 1023)

// Thresholds
const float TEMP_THRESHOLD_C  = 27.0; // if temperature go above this number, the alarm will triggering
const int   DIST_THRESHOLD_CM = 50; // object must be closer than this many centimeter for proximity alert

// Tone frequencies
const int BEEP_FREQ_DIST = 1000;  // frequency in Hz that buzzer use when something is too close
const int BEEP_FREQ_TILT = 1500;  // frequency for when the tilt switch has been activated
const int BEEP_FREQ_TEMP = 2000;  // highest frequency, used when temperature is exceeding the threshold

void setup() {
  pinMode(TRIG_PIN, OUTPUT); // trig pin must be output so we can sending pulse
  pinMode(ECHO_PIN, INPUT); // echo pin is input because we listening for returning pulse
  pinMode(BUZZER_PIN, OUTPUT); // buzzer need output mode so we can drive it
  pinMode(TILT_PIN, INPUT_PULLUP);  // use internal pullup so pin is HIGH normally, go LOW when tilt switch close to GND

  pinMode(RGB_RED,   OUTPUT); // set red led pin as output
  pinMode(RGB_GREEN, OUTPUT); // set green led pin as output
  pinMode(RGB_BLUE,  OUTPUT); // set blue led pin as output

  setRGB(0, 0, 0); // turn all LED color off at the starting
  Serial.begin(9600); // start serial at 9600 baud for the debugging print
}

void loop() {
  float tempC    = readThermistorTemp(); // call function to get current temperature reading
  long  distCm   = readDistance(); // call function to measure how far away object is
  bool  tilted   = (digitalRead(TILT_PIN) == LOW); // if pin reading LOW then tilt switch is closed meaning device got tilted

  // print all sensor value to serial monitor for debug purpose
  Serial.print("Temp: "); Serial.print(tempC);
  Serial.print(" C | Dist: "); Serial.print(distCm);
  Serial.print(" cm | Tilted: "); Serial.println(tilted ? "YES" : "NO");

  bool buzzerActive = false; // start by assuming buzzer should not be playing
  int  buzzerFreq   = 0; // frequency start at zero until some condition set it

  // --- Priority: Temperature (highest) > Tilt > Distance ---
  // temperature alert check first because it having highest priority in system
  if (tempC > TEMP_THRESHOLD_C) {
    setRGB(255, 0, 0);       // turn LED to full red for showing temperature danger
    buzzerFreq   = BEEP_FREQ_TEMP; // set frequency to the temp alert tone
    buzzerActive = true; // mark that buzzer should be active this cycle
  } else {
    setRGB(0, 0, 0);         // if temperature is okay then LED should turning off
  }

  // tilt check come after temperature, it will overwrite the frequency if both happening
  if (tilted) {
    buzzerFreq   = BEEP_FREQ_TILT; // overwrite frequency with tilt tone
    buzzerActive = true; // buzzer should still be active
  }

  // distance check is lowest priority, only set frequency if nothing else already making buzzer active
  if (distCm > 0 && distCm < DIST_THRESHOLD_CM) {
    if (!buzzerActive) buzzerFreq = BEEP_FREQ_DIST; // only use dist frequency if higher priority not already setting it
    buzzerActive = true; // but buzzer should active regardless
  }

  // now actually control the buzzer based on what conditions was triggered above
  if (buzzerActive) {
    tone(BUZZER_PIN, buzzerFreq, 150); // play tone at chosen frequency for 150 millisecond
    delay(300); // wait a bit so tone can be heard properly
    noTone(BUZZER_PIN); // stop the tone after the delay
  } else {
    noTone(BUZZER_PIN); // make sure buzzer is silent if no alert condition is present
  }

  delay(200); // small delay before next loop iteration so we not reading too fast
}

// ---- Helper Functions ----

float readThermistorTemp() {
  int   raw     = analogRead(THERMISTOR_PIN); // read raw ADC value from thermistor pin (0 to 1023)
  float voltage = (float)raw / ADC_MAX; // convert raw value to a ratio between 0.0 and 1.0

  // using simplified Steinhart-Hart equation (also calling beta equation) to find resistance then temperature
  float resistance = SERIES_RESISTOR * (1.0 / voltage - 1.0); // calculate thermistor resistance from voltage divider math

  float steinhart;
  steinhart  = resistance / NOMINAL_RESISTANCE;      // divide current resistance by nominal resistance to get R/R0 ratio
  steinhart  = log(steinhart);                        // take natural log of the ratio as the equation requiring
  steinhart /= B_COEFFICIENT;                         // divide by beta coefficient value from datasheet
  steinhart += 1.0 / (NOMINAL_TEMP + 273.15);         // add inverse of nominal temp converted to kelvin
  steinhart  = 1.0 / steinhart;                       // take inverse of whole thing to get temperature in kelvin
  float tempC = steinhart - 273.15;                   // subtract 273.15 to convert from kelvin back to celsius

  return tempC; // return the final celsius temperature value
}

long readDistance() {
  digitalWrite(TRIG_PIN, LOW); // make sure trig pin is low before we start the pulse
  delayMicroseconds(2); // wait 2 microsecond to ensure pin is settle at LOW
  digitalWrite(TRIG_PIN, HIGH); // send the trigger pulse by going HIGH
  delayMicroseconds(10); // hold HIGH for 10 microsecond as sensor is requiring
  digitalWrite(TRIG_PIN, LOW); // bring trigger back LOW to ending the pulse

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // measure how long echo pin stay HIGH, timeout after 30ms if nothing return
  if (duration == 0) return 999;                   // if no echo come back then return big number meaning out of range

  long distCm = duration * 0.034 / 2; // convert duration to centimeter using speed of sound, divide by 2 for round trip
  return distCm; // send back the distance value
}

void setRGB(int r, int g, int b) {
  analogWrite(RGB_RED,   r); // write red brightness value to the red pin using PWM
  analogWrite(RGB_GREEN, g); // write green brightness to green pin
  analogWrite(RGB_BLUE,  b); // write blue brightness to blue pin
}