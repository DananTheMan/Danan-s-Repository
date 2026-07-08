#define greenPin 2
#define buzzerPin 8

void beepBlink(int speed) {
  digitalWrite(greenPin, HIGH);
  tone(buzzerPin, 2000, speed);
  delay(speed);
  digitalWrite(greenPin, LOW);
  delay(speed);
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
}

void loop() {
  // Phase 1: steady beeping/blinking for 5 seconds
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    beepBlink(200);  // fixed speed
  }

  for (int speed = 200; speed >= 20; speed -= 10) {
    beepBlink(speed);
  }
}