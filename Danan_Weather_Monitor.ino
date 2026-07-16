/*
  WEATHER WARNING RADIO STATION
  ------------------------------
  Retro-style emergency weather alert desk unit.

  Hardware:
    - DHT11 or DHT22 temperature/humidity sensor -> pin 2
    - 16x2 I2C LCD (e.g. PCF8574 backpack)         -> SDA/SCL (A4/A5 on Uno)
    - Servo (SAFE / CAUTION / DANGER needle)       -> pin 9
    - 3x passive buzzers                           -> pins 3, 5, 6

  Libraries needed (Install via Library Manager):
    - DHT sensor library (by Adafruit)
    - Adafruit Unified Sensor (dependency of the above)
    - LiquidCrystal I2C (by Frank de Brabander)
    - Servo (built-in)
    - Wire (built-in)

  NOTE: I2C LCD backpacks commonly use address 0x27 or 0x3F.
  If the display shows nothing/garbage, run an I2C scanner sketch
  to find your address and change LCD_ADDR below.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

// ---------- PIN CONFIG ----------
#define DHTPIN 6
#define DHTTYPE DHT22     // change to DHT22 if that's what you have

#define LCD_ADDR 0x27      // change to 0x3F if 0x27 doesn't work
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

Servo needle;
const int SERVO_PIN = 3;

const int BUZZER1 = 9;
const int BUZZER2 = 10;
const int BUZZER3 = 11;

DHT dht(DHTPIN, DHTTYPE);

// ---------- THRESHOLDS (tune to taste) ----------
const float TEMP_WARNING   = 30.0;  // °C
const float TEMP_ADVISORY  = 27.0;
const float HUM_WARNING    = 70.0;  // %
const float HUM_ADVISORY   = 60.0;

// Servo angles for the gauge
const int ANGLE_SAFE    = 45;
const int ANGLE_CAUTION = 90;
const int ANGLE_DANGER  = 135;

enum Status { SAFE, ADVISORY, WARNING };
Status lastStatus = SAFE;

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 3000; // read sensor every 3s

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  needle.attach(SERVO_PIN);
  needle.write(ANGLE_SAFE);

  pinMode(BUZZER1, OUTPUT);
  pinMode(BUZZER2, OUTPUT);
  pinMode(BUZZER3, OUTPUT);

  dht.begin();

  bootSequence();
}

void loop() {
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();

    if (isnan(temp) || isnan(hum)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SENSOR ERROR");
      return;
    }

    Status current = getStatus(temp, hum);

    // If status just changed, play the alert tone
    if (current != lastStatus) {
      if (current == WARNING) {
        alertSiren();
      } else if (current == ADVISORY) {
        alertChirp();
      }
      moveNeedle(current);
      lastStatus = current;
    }

    displayReport(temp, hum, current);
  }
}

// ---------- STATUS LOGIC ----------
Status getStatus(float temp, float hum) {
  if (temp >= TEMP_WARNING || hum >= HUM_WARNING) return WARNING;
  if (temp >= TEMP_ADVISORY || hum >= HUM_ADVISORY) return ADVISORY;
  return SAFE;
}

// ---------- DISPLAY ----------
void displayReport(float temp, float hum, Status s) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(hum, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  switch (s) {
    case SAFE:     lcd.print("STATUS: NORMAL"); break;
    case ADVISORY: lcd.print("** ADVISORY **"); break;
    case WARNING:  lcd.print("!! WARNING !!"); break;
  }
}

// ---------- SERVO GAUGE ----------
void moveNeedle(Status s) {
  int target;
  switch (s) {
    case SAFE:     target = ANGLE_SAFE; break;
    case ADVISORY: target = ANGLE_CAUTION; break;
    case WARNING:  target = ANGLE_DANGER; break;
  }
  int current = needle.read();
  int step = (target > current) ? 1 : -1;
  for (int a = current; a != target; a += step) {
    needle.write(a);
    delay(8);
  }
  needle.write(target);
}

// ---------- ALERT TONES ----------
void alertChirp() {
  for (int i = 0; i < 2; i++) {
    tone(BUZZER1, 660, 150);
    delay(180);
    tone(BUZZER2, 880, 150);
    delay(180);
  }
  noTone(BUZZER1);
  noTone(BUZZER2);
}

void alertSiren() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EMERGENCY ALERT");
  lcd.setCursor(0, 1);
  lcd.print("DO NOT IGNORE");

  for (int rep = 0; rep < 3; rep++) {
    tone(BUZZER1, 400);
    tone(BUZZER2, 500);
    tone(BUZZER3, 600);
    delay(300);
    tone(BUZZER1, 700);
    tone(BUZZER2, 800);
    tone(BUZZER3, 900);
    delay(300);
  }
  noTone(BUZZER1);
  noTone(BUZZER2);
  noTone(BUZZER3);
}

// ---------- BOOT SEQUENCE ----------
void bootSequence() {
  lcd.setCursor(0, 0);
  lcd.print("WX RADIO STATION");
  lcd.setCursor(0, 1);
  lcd.print("Booting...");

  int notes[] = {262, 330, 392}; // C4 E4 G4
  int buzzers[] = {BUZZER1, BUZZER2, BUZZER3};

  for (int i = 0; i < 3; i++) {
    tone(buzzers[i], notes[i], 200);
    delay(220);
    noTone(buzzers[i]);
  }

  delay(500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Online");
  delay(1000);
}