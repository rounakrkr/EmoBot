#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ── Eye Expression Types ───────────────────────────────────────
enum EyeExpression {
  EYE_NORMAL,
  EYE_ATTENTIVE,
  EYE_HAPPY,
  EYE_HAPPY_BLINK,
  EYE_SAD,
  EYE_SURPRISED,
  EYE_BLINK
};

// ── Internal helpers ───────────────────────────────────────────
// Each eye is drawn on a 128x64 canvas.
// We draw a stylised circular iris + pupil.

static void _drawNormalEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  // Outer eye shape (rounded rect)
  d.fillRoundRect(14, 8, 100, 48, 20, WHITE);
  // Pupil (black circle inside white)
  d.fillCircle(64, 32, 18, BLACK);
  // Shine dot
  d.fillCircle(72, 26, 4, WHITE);
  d.display();
}

static void _drawAttentiveEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  // Wider open eye
  d.fillRoundRect(8, 4, 112, 56, 22, WHITE);
  // Larger pupil
  d.fillCircle(64, 32, 22, BLACK);
  d.fillCircle(73, 25, 5, WHITE);
  d.display();
}

// Happy eyes - thoda aur cute
static void _drawHappyEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  // Crescent / squint — draw a filled rect then cut top with black arc
  d.fillRoundRect(14, 20, 100, 38, 18, WHITE);
  // Arc cut at top to make it look squinted/smiling
  d.fillCircle(64, 20, 26, BLACK);
  d.display();
}

static void _drawHappyBlinkEye(Adafruit_SSD1306& d) {
  d.clearDisplay();
  // Thin arc = happy closed blink
  for (int x = 14; x < 114; x++) {
    int y = 32 + (int)(8 * sin(((x - 64) / 50.0) * PI));
    d.drawPixel(x, y,   WHITE);
    d.drawPixel(x, y+1, WHITE);
    d.drawPixel(x, y+2, WHITE);
  }
  d.display();
}

static void _drawSadEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  // Droopy: normal eye but pupil offset down, inner brow furrowed
  d.fillRoundRect(14, 14, 100, 44, 18, WHITE);
  d.fillCircle(64, 38, 16, BLACK);
  d.fillCircle(71, 33, 3, WHITE);
  // Inner corner droop (dark triangle at top-inner)
  int innerX = left ? 14 : 100;
  d.fillTriangle(innerX, 14, innerX + (left ? 30 : -30), 14,
                  innerX + (left ? 10 : -10), 30, BLACK);
  d.display();
}

static void _drawBlinkEye(Adafruit_SSD1306& d) {
  d.clearDisplay();
  // Simple horizontal line = closed eye
  d.fillRect(14, 30, 100, 4, WHITE);
  d.display();
}

static void _drawSurprisedEye(Adafruit_SSD1306& d) {
  d.clearDisplay();
  // Very round, wide
  d.fillCircle(64, 32, 30, WHITE);
  d.fillCircle(64, 32, 16, BLACK);
  d.fillCircle(72, 26, 5, WHITE);
  d.display();
}

// ── Public API ─────────────────────────────────────────────────

void eyeSetup(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  // Second I2C bus for right eye
  Wire.begin();

  if (!left.begin(SSD1306_SWITCHCAPVCC, LEFT_EYE_ADDR)) {
    Serial.println("[EYES] Left OLED init FAILED");
  }
  if (!right.begin(SSD1306_SWITCHCAPVCC, RIGHT_EYE_ADDR)) {
    Serial.println("[EYES] Right OLED init FAILED");
  }

  left.setTextColor(WHITE);
  right.setTextColor(WHITE);
  left.clearDisplay();  left.display();
  right.clearDisplay(); right.display();
  Serial.println("[EYES] OLEDs initialised");
}

void eyeShowExpression(Adafruit_SSD1306& left, Adafruit_SSD1306& right, EyeExpression expr) {
  switch (expr) {
    case EYE_NORMAL:
      _drawNormalEye(left,  true);
      _drawNormalEye(right, false);
      break;
    case EYE_ATTENTIVE:
      _drawAttentiveEye(left,  true);
      _drawAttentiveEye(right, false);
      break;
    case EYE_HAPPY:
      _drawHappyEye(left,  true);
      _drawHappyEye(right, false);
      break;
    case EYE_HAPPY_BLINK:
      _drawHappyBlinkEye(left);
      _drawHappyBlinkEye(right);
      break;
    case EYE_SAD:
      _drawSadEye(left,  true);
      _drawSadEye(right, false);
      break;
    case EYE_SURPRISED:
      _drawSurprisedEye(left);
      _drawSurprisedEye(right);
      break;
    case EYE_BLINK:
      _drawBlinkEye(left);
      _drawBlinkEye(right);
      break;
  }
}

// Standard blink: close → pause → reopen
void eyeBlink(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  eyeShowExpression(left, right, EYE_BLINK);
  delay(150);
  eyeShowExpression(left, right, EYE_NORMAL);
}

// Double blink
void eyeDoubleBlink(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  eyeBlink(left, right);
  delay(80);
  eyeBlink(left, right);
}

// Slow, heavy blink (used for NEGATIVE/sad state)
// Blink step by step
void eyeSoftBlink(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  // Step 1 - slightly close
  for (int i = 0; i < 3; i++) {
    left.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    right.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    left.display(); right.display();
    delay(30);
  }
  // Step 2 - closed
  left.fillRect(30, 35, 68, 4, WHITE);
  right.fillRect(30, 35, 68, 4, WHITE);
  left.display(); right.display();
  delay(80);
  // Step 3 - open slowly
  for (int i = 2; i >= 0; i--) {
    left.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    right.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    left.display(); right.display();
    delay(30);
  }
}