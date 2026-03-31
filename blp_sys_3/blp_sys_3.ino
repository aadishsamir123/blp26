void setup() { // we ensure all the pins are in the correct configuration
  pinMode(2, INPUT_PULLUP);
  pinMode(8, OUTPUT);
}

int i = 1;

void loop() { // in this loop all we do is check that whether the tilt sensor is tilt or not.
  if (digitalRead(2) == LOW) { // tilt detected
    while (i <= 5) {
      tone(8, 400); // speaker noise
      delay(700);
      noTone(8); // speaker no noise
      delay(700);
      tone(8, 400);
      delay(700);
      noTone(8);
      delay(700);
      i++;
    }
    i = 1;
  } else { // no tilt detected
    noTone(8);
  }
}