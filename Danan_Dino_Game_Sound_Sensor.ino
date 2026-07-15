/*
  DINO GAME — 16x2 I2C LCD + Ultrasonic Sensor
  -----------------------------------------------------------------
  HARDWARE ASSUMED:
    - Standard 16x2 character LCD (HD44780-compatible),
      each character cell is 5x8 dots, with an I2C backpack
      (PCF8574-based, address usually 0x27 or 0x3F)
    - HC-SR04 ultrasonic distance sensor
    - Arduino Uno

  LIBRARY NEEDED (Library Manager):
    - "LiquidCrystal I2C" (Frank de Brabander or Marco Schwartz fork)

  IF THE SCREEN SHOWS GARBAGE OR NOTHING:
    Try address 0x3F instead of 0x27 (run an I2C scanner sketch
    if you're not sure which your backpack uses).

  WIRING:
    LCD backpack: SDA -> A4, SCL -> A5, VCC -> 5V, GND -> GND
    HC-SR04:      VCC -> 5V, GND -> GND
                  TRIG -> pin 9
                  ECHO -> pin 10

  HOW THE DISPLAY IS USED:
    The dino lives in a fixed column. The bottom character row
    is the "ground" — the dino stands there by default. The top
    character row is the "jump" position.

    Obstacles scroll in from the right, one per column, and are
    one of two kinds:
      - Cactus (ground obstacle): blocks the BOTTOM row. You must
        be jumping (hand close) to clear it.
      - Pterodactyl (air obstacle): blocks the TOP row. You must
        be on the ground (hand away) to clear it.

  GAMEPLAY:
    Hold your hand within JUMP_DISTANCE_CM of the sensor and the
    dino jumps up and STAYS up for as long as your hand stays
    close. Move your hand farther away and the dino comes back
    down to the ground immediately. There's no gravity/arc timing
    to fight — it's a simple "hand close = up, hand away = down"
    switch, so you control exactly when the dino is in the air.

    Hitting an obstacle in whichever row the dino is currently in
    ends the game; score is how many obstacles you clear.

  DIFFICULTY / UNPREDICTABILITY:
    - Gap between obstacles is randomized within a range instead
      of a fixed spacing, so you can't settle into a rhythm.
    - Only ever ONE obstacle at a time — no back-to-back combos —
      so there's always a fair, guaranteed gap to react in.
    - The game starts slow and steps up in speed every 10 points,
      instead of speeding up continuously, so the difficulty ramp
      is predictable in when it happens even if the obstacles
      aren't.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t LCD_ADDRESS = 0x27; // change to 0x3F if screen doesn't work
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// ---------- Pin setup ----------
const uint8_t TRIG_PIN = 9;
const uint8_t ECHO_PIN = 10;

// ---------- Game constants ----------
const uint8_t GRID_W = 16;            // 16 character columns
const uint8_t DINO_COL = 1;           // dino stays in this fixed column
const uint8_t GROUND_ROW = 1;         // bottom character row = ground
const uint8_t JUMP_ROW = 0;           // top character row = up in the air

const float JUMP_DISTANCE_CM = 15.0;  // hand closer than this = jump / stay up

// --- speed ramp: starts slow, steps faster every POINTS_PER_LEVEL points ---
const unsigned long START_TICK_MS = 220;   // starting speed (slow)
const unsigned long MIN_TICK_MS = 90;      // fastest the game will ever get
const unsigned long TICK_MS_PER_LEVEL = 15;// ms shaved off per level
const uint8_t POINTS_PER_LEVEL = 10;       // score needed to reach the next level

// --- spacing: randomized so obstacle rhythm can't be memorized ---
// MIN_SPACING is kept generously wide so there's always a fair, single
// clear obstacle to react to — never two things back-to-back.
const uint8_t MIN_SPACING = 5;
const uint8_t MAX_SPACING = 9;

// ---------- Custom character slots ----------
const uint8_t DINO_CHAR_SLOT = 0;
const uint8_t CACTUS_CHAR_SLOT = 1;
const uint8_t PTERODACTYL_CHAR_SLOT = 2;

byte dinoGlyph[8] = {
  0b00110,
  0b00111,
  0b00101,
  0b00111,
  0b01110,
  0b11110,
  0b01100,
  0b01100
};

byte cactusGlyph[8] = {
  0b00100,
  0b10101,
  0b10101,
  0b10111,
  0b00100,
  0b00100,
  0b00100,
  0b11111
};

byte pterodactylGlyph[8] = {
  0b00000,
  0b10001,
  0b11011,
  0b01110,
  0b00100,
  0b00000,
  0b00000,
  0b00000
};

// ---------- Game state ----------
// obstacleRow[x]: -1 = nothing in this column
//                  0 = pterodactyl, blocks TOP row (JUMP_ROW)
//                  1 = cactus, blocks BOTTOM row (GROUND_ROW)
int8_t obstacleRow[GRID_W];

uint8_t dinoCharRow = GROUND_ROW; // current row the dino occupies

unsigned long lastTick = 0;
bool gameOver = false;
int score = 0;

int8_t columnsUntilSpawn = MIN_SPACING; // countdown to next obstacle spawn

// ---------- Ultrasonic distance read ----------
float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000UL); // 25ms timeout (~4m range)
  if (duration == 0) return 999.0; // no echo = treat as "far away"

  return duration * 0.0343 / 2.0; // speed of sound conversion to cm
}

// ---------- Game setup / reset ----------
void resetGame() {
  dinoCharRow = GROUND_ROW;
  score = 0;
  gameOver = false;

  for (uint8_t i = 0; i < GRID_W; i++) obstacleRow[i] = -1;

  columnsUntilSpawn = random(MIN_SPACING, MAX_SPACING + 1);
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.createChar(DINO_CHAR_SLOT, dinoGlyph);
  lcd.createChar(CACTUS_CHAR_SLOT, cactusGlyph);
  lcd.createChar(PTERODACTYL_CHAR_SLOT, pterodactylGlyph);

  randomSeed(analogRead(A0)); // unused analog pin for entropy

  resetGame();
}

// ---------- Current game speed based on score ----------
unsigned long currentTickMs() {
  uint8_t level = score / POINTS_PER_LEVEL;
  unsigned long speedUp = (unsigned long)level * TICK_MS_PER_LEVEL;
  if (speedUp >= START_TICK_MS - MIN_TICK_MS) return MIN_TICK_MS;
  return START_TICK_MS - speedUp;
}

// ---------- Shift the world left, spawn new obstacles, score ----------
void scrollWorld() {
  // an obstacle sitting in column 0 is about to scroll off — the dino
  // (fixed at column 1) has fully cleared it, so award the point
  if (obstacleRow[0] != -1) {
    score++;
  }

  for (uint8_t i = 0; i < GRID_W - 1; i++) {
    obstacleRow[i] = obstacleRow[i + 1];
  }
  obstacleRow[GRID_W - 1] = -1;

  columnsUntilSpawn--;
  if (columnsUntilSpawn <= 0) {
    obstacleRow[GRID_W - 1] = random(0, 2); // 0 = pterodactyl (top), 1 = cactus (bottom)
    columnsUntilSpawn = random(MIN_SPACING, MAX_SPACING + 1);
  }
}

// ---------- Collision check ----------
bool checkCollision() {
  int8_t blocked = obstacleRow[DINO_COL];
  if (blocked == -1) return false;
  return (dinoCharRow == blocked);
}

// ---------- Draw current frame ----------
void draw() {
  lcd.clear();

  // draw obstacles first
  for (uint8_t x = 0; x < GRID_W; x++) {
    if (obstacleRow[x] == -1) continue;
    lcd.setCursor(x, obstacleRow[x]);
    lcd.write(byte(obstacleRow[x] == 0 ? PTERODACTYL_CHAR_SLOT : CACTUS_CHAR_SLOT));
  }

  // draw the dino at whichever row it currently occupies
  lcd.setCursor(DINO_COL, dinoCharRow);
  lcd.write(byte(DINO_CHAR_SLOT)); // overwrites any obstacle drawn in this cell
}

// ---------- Game over message ----------
void playGameOverAnimation() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("GAME OVER");
  lcd.setCursor(4, 1);
  lcd.print("Score: ");
  lcd.print(score);
  delay(2000);
}

void loop() {
  unsigned long now = millis();
  if (now - lastTick < currentTickMs()) return; // pace the game; steps faster every level
  lastTick = now;

  if (gameOver) {
    playGameOverAnimation();
    resetGame();
    return;
  }

  // --- read sensor & set dino row directly ---
  // hand close -> jump up and STAY up; hand away -> back on the ground.
  // No velocity/gravity: this is a direct switch, not a timed hop.
  float distance = readDistanceCM();
  dinoCharRow = (distance < JUMP_DISTANCE_CM) ? JUMP_ROW : GROUND_ROW;

  // --- advance world ---
  scrollWorld();

  // --- collision ---
  if (checkCollision()) {
    gameOver = true;
  }

  draw();
}
