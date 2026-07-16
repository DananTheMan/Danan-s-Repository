#include <Servo.h>

Servo myServo;

int servoPin = 5;     
int trigPin = 11;     
int echoPin = 12;     

int pos = 0;
long duration;
float distanceCm;

void setup() {
  myServo.attach(servoPin);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  Serial.begin(9600);
}

void loop() {
  for (pos = 0; pos <= 180; pos += 1) {
    myServo.write(pos);
    delay(30);              
    distanceCm = getDistance();
    printReading();
  }

  // Sweep back from 180 to 0 degrees
  for (pos = 180; pos >= 0; pos -= 1) {
    myServo.write(pos);
    delay(30);
    distanceCm = getDistance();
    printReading();
  }
}

float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  
  duration = pulseIn(echoPin, HIGH, 30000); 

  if (duration == 0) {
    return -1; 
  }

  float distance = duration * 0.0343 / 2;
  return distance;
}

void printReading() {
  Serial.print("Angle: ");
  Serial.print(pos);
  Serial.print(" deg, Distance: ");
  if (distanceCm < 0) {
    Serial.println("Out of range");v
  } else {
    Serial.print(distanceCm);
    Serial.println(" cm");
  }
}