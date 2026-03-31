// initialise all the pins
int trigPin = 9;//makes them a variable and assigns them respective pins
int echoPin = 10;
int buzzer = 8;

long duration;//creates variable duration and uses long instead of int since number can be very LARGE
int distance;//creates distance variable

void setup() {
  // making all the pins correct type
  pinMode(trigPin, OUTPUT);//sets pin 9 as output - send out ultrasonic pulse
  pinMode(echoPin, INPUT);//sets pin 10 input - listens for returning pulse
  pinMode(buzzer, OUTPUT);//sets pin 8 to output - buzzer
  Serial.begin(9600);//starts serial monitor at 9600 baud - optimal for arduino model
}

void loop() {
  // make ultrasonic sensor work (trigger it)
  digitalWrite(trigPin, LOW);//clears trigger pin
  delayMicroseconds(2);// waits 2 miliseconds - clear signal before pulse sends
//sends 10 microsecond HIGH pulse which triggers ultrasonic sensor to fire a sound wave
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
//listens on pin 10 and measures how long it takes for the sound wave to come back in microseconds
  duration = pulseIn(echoPin, HIGH);//stores in duration

  // distance calculation
  distance = duration * 0.034 / 2;

  // print the distance to serial monitor
  Serial.println(distance);

  // make the buzzer beep if something is within 50cm is detected
  if (distance > 0 && distance < 50) {
    tone(buzzer, 200);//plays 200hz sound
    delay(3000);//plays for 3 seconds
  } else {
    noTone(buzzer);//if nothing is within 50cm buzzer stays silent
  }
}