// init all the pins
int trigPin = 9;
int echoPin = 10;
int buzzer = 8;

long duration;
int distance;

void setup() {
  // making all the pins correct type
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzer, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // make ultrasonic sensor work (trigger it)
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);

  // distance calculation
  distance = duration * 0.034 / 2;

  // print the distance
  Serial.println(distance);

  // make the buzzer beep if distance is detected
  if (distance > 0 && distance < 50) {
    tone(buzzer, 200);
    delay(3000);
  } else {
    noTone(buzzer);
  }
}