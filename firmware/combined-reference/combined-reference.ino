#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ==========================================
// DESK-BUDDY: DISPLAY + MOTION + AUDIO
// ==========================================

// Display pins
#define TFT_CS   D0
#define TFT_DC   D2
#define TFT_MOSI D10
#define TFT_SCLK D8

// Motion sensor
#define PIR_PIN D1

// DFPlayer pins
#define DF_RX D7
#define DF_TX D6

// Display
Adafruit_ST7789 tft = Adafruit_ST7789(
  TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, -1
);

// Timing
const unsigned long AUDIO_COOLDOWN = 20000;
const unsigned long BLINK_DURATION = 180;

unsigned long lastAudioTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long nextBlinkInterval = 3000;

bool hasPlayed = false;
bool previousMotion = false;
bool eyesClosed = false;

int nextAudio = 1;

// ==========================================
// FACE
// ==========================================

void drawFace(bool closed) {
  tft.fillScreen(ST77XX_BLACK);

  uint16_t eyeColor = ST77XX_WHITE;

  if (closed) {
    // Closed eyes
    tft.fillRoundRect(48, 118, 55, 7, 3, eyeColor);
    tft.fillRoundRect(137, 118, 55, 7, 3, eyeColor);
  } else {
    // Open eyes
    tft.fillRoundRect(48, 90, 55, 65, 18, eyeColor);
    tft.fillRoundRect(137, 90, 55, 65, 18, eyeColor);
  }

  // Smile
  tft.drawRoundRect(92, 190, 56, 28, 12, eyeColor);
  tft.fillRect(91, 190, 58, 13, ST77XX_BLACK);
}

void updateFace(unsigned long now) {
  if (!eyesClosed &&
      now - lastBlinkTime >= nextBlinkInterval) {

    eyesClosed = true;
    lastBlinkTime = now;
    drawFace(true);
  }

  if (eyesClosed &&
      now - lastBlinkTime >= BLINK_DURATION) {

    eyesClosed = false;
    lastBlinkTime = now;

    nextBlinkInterval = random(2500, 6000);

    drawFace(false);
  }
}

// ==========================================
// DFPLAYER
// ==========================================

void sendDFPlayerCommand(uint8_t command, uint16_t parameter) {
  uint8_t packet[10] = {
    0x7E, 0xFF, 0x06, command, 0x00,
    (uint8_t)(parameter >> 8),
    (uint8_t)(parameter & 0xFF),
    0x00, 0x00, 0xEF
  };

  uint16_t checksum = 0;

  for (int i = 1; i <= 6; i++) {
    checksum += packet[i];
  }

  checksum = -checksum;

  packet[7] = checksum >> 8;
  packet[8] = checksum & 0xFF;

  Serial1.write(packet, 10);
  Serial1.flush();
}

void playNextAudio(unsigned long now) {
  Serial.print("Playing MP3: ");
  Serial.println(nextAudio);

  sendDFPlayerCommand(0x12, nextAudio);

  lastAudioTime = now;
  hasPlayed = true;

  nextAudio++;

  if (nextAudio > 3) {
    nextAudio = 1;
  }
}

// ==========================================
// MOTION
// ==========================================

void updateMotion(unsigned long now) {
  bool motion = digitalRead(PIR_PIN) == HIGH;

  bool newMotion = motion && !previousMotion;

  bool cooldownComplete =
      !hasPlayed ||
      (now - lastAudioTime >= AUDIO_COOLDOWN);

  if (newMotion && cooldownComplete) {
    Serial.println("MOTION DETECTED!");

    playNextAudio(now);
  }

  previousMotion = motion;
}

// ==========================================
// SETUP
// ==========================================

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);

  Serial.println("Desk-Buddy starting...");

  // Start display
  tft.init(240, 320);
  tft.setRotation(0);

  randomSeed(micros());

  drawFace(false);

  Serial.println("Display initialized.");

  // Start DFPlayer
  Serial1.begin(9600, SERIAL_8N1, DF_RX, DF_TX);

  delay(3000);

  sendDFPlayerCommand(0x06, 20);

  Serial.println("DFPlayer initialized.");
  Serial.println("Waiting for motion...");

  lastBlinkTime = millis();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  unsigned long now = millis();

  updateFace(now);
  updateMotion(now);
}
