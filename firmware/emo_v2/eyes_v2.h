/**
 * Emo V2 — OLED Eye Expressions
 * Added: EYE_CURIOUS, EYE_ANGRY, EYE_THINKING
 * Kept: all existing good expressions from V1
 */
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "../config.h"

// ── Expression types ────────────────────────────────
enum EyeExpression {
  EYE_NORMAL,
  EYE_ATTENTIVE,
  EYE_HAPPY,
  EYE_HAPPY_BLINK,
  EYE_SAD,
  EYE_SURPRISED,
  EYE_BLINK,
  EYE_CURIOUS,     // V2 new
  EYE_ANGRY,       // V2 new
  EYE_THINKING     // V2 new (looking up)
};

// ── Draw functions ───────────────────────────────────

static void _drawNormalEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  d.fillRoundRect(14, 8, 100, 48, 20, WHITE);
  d.fillCircle(64, 32, 18, BLACK);
  d.fillCircle(72, 26, 4, WHITE);
  d.display();
}

static void _drawAttentiveEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  d.fillRoundRect(8, 4, 112, 56, 22, WHITE);
  d.fillCircle(64, 32, 22, BLACK);
  d.fillCircle(73, 25, 5, WHITE);
  d.display();
}

static void _drawHappyEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  d.fillRoundRect(14, 20, 100, 38, 18, WHITE);
  d.fillCircle(64, 20, 26, BLACK);
  d.display();
}

static void _drawHappyBlinkEye(Adafruit_SSD1306& d) {
  d.clearDisplay();
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
  d.fillRoundRect(14, 14, 100, 44, 18, WHITE);
  d.fillCircle(64, 38, 16, BLACK);
  d.fillCircle(71, 33, 3, WHITE);
  int innerX = left ? 14 : 100;
  d.fillTriangle(innerX, 14, innerX + (left ? 30 : -30), 14,
                  innerX + (left ? 10 : -10), 30, BLACK);
  d.display();
}

static void _drawBlinkEye(Adafruit_SSD1306& d) {
  d.clearDisplay();
  d.fillRect(14, 30, 100, 4, WHITE);
  d.display();
}

static void _drawSurprisedEye(Adafruit_SSD1306& d) {
  d.clearDisplay();
  d.fillCircle(64, 32, 30, WHITE);
  d.fillCircle(64, 32, 16, BLACK);
  d.fillCircle(72, 26, 5, WHITE);
  d.display();
}

// V2 NEW: Curious — one eyebrow raised effect, pupil offset
static void _drawCuriousEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  // Slightly wide eye
  d.fillRoundRect(10, 10, 108, 48, 20, WHITE);
  // Pupil offset to center-up (looking curious)
  d.fillCircle(64, 28, 19, BLACK);
  d.fillCircle(72, 22, 4, WHITE);
  // Raised inner corner (curious brow)
  int bx = left ? 14 : 86;
  d.fillTriangle(bx, 10, bx + (left ? 28 : -28), 10,
                  bx + (left ? 6 : -6), 3, BLACK);
  d.display();
}

// V2 NEW: Angry — furrowed inner brow, squinted
static void _drawAngryEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  // Squinted base
  d.fillRoundRect(14, 18, 100, 38, 14, WHITE);
  // Pupil
  d.fillCircle(64, 34, 15, BLACK);
  d.fillCircle(71, 29, 3, WHITE);
  // Furrowed brow triangle (inner corner, angled inward)
  int bx = left ? 14 : 100;
  d.fillTriangle(bx, 18,
                  bx + (left ? 40 : -40), 18,
                  bx + (left ? 10 : -10), 5, BLACK);
  d.display();
}

// V2 NEW: Thinking — pupil looking up-left (as if thinking)
static void _drawThinkingEye(Adafruit_SSD1306& d, bool left) {
  d.clearDisplay();
  d.fillRoundRect(14, 8, 100, 48, 20, WHITE);
  // Pupil offset up and slightly to inner side (looking up)
  int px = left ? 52 : 76;
  d.fillCircle(px, 22, 16, BLACK);
  d.fillCircle(px + 6, 18, 3, WHITE);
  d.display();
}

// ── Public API ───────────────────────────────────────

void eyeSetup(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  Wire.begin();
  if (!left.begin(SSD1306_SWITCHCAPVCC, LEFT_EYE_ADDR))
    Serial.println("[EYES] Left OLED FAILED");
  if (!right.begin(SSD1306_SWITCHCAPVCC, RIGHT_EYE_ADDR))
    Serial.println("[EYES] Right OLED FAILED");
  left.setTextColor(WHITE);
  right.setTextColor(WHITE);
  left.clearDisplay();  left.display();
  right.clearDisplay(); right.display();
  Serial.println("[EYES] OLEDs initialised");
}

void eyeShowExpression(Adafruit_SSD1306& left, Adafruit_SSD1306& right, EyeExpression expr) {
  switch (expr) {
    case EYE_NORMAL:
      _drawNormalEye(left, true);  _drawNormalEye(right, false);  break;
    case EYE_ATTENTIVE:
      _drawAttentiveEye(left, true); _drawAttentiveEye(right, false); break;
    case EYE_HAPPY:
      _drawHappyEye(left, true);   _drawHappyEye(right, false);   break;
    case EYE_HAPPY_BLINK:
      _drawHappyBlinkEye(left);    _drawHappyBlinkEye(right);     break;
    case EYE_SAD:
      _drawSadEye(left, true);     _drawSadEye(right, false);     break;
    case EYE_SURPRISED:
      _drawSurprisedEye(left);     _drawSurprisedEye(right);      break;
    case EYE_BLINK:
      _drawBlinkEye(left);         _drawBlinkEye(right);          break;
    case EYE_CURIOUS:
      _drawCuriousEye(left, true); _drawCuriousEye(right, false); break;
    case EYE_ANGRY:
      _drawAngryEye(left, true);   _drawAngryEye(right, false);   break;
    case EYE_THINKING:
      _drawThinkingEye(left, true); _drawThinkingEye(right, false); break;
  }
}

void eyeBlink(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  eyeShowExpression(left, right, EYE_BLINK);
  delay(150);
  eyeShowExpression(left, right, EYE_NORMAL);
}

void eyeDoubleBlink(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  eyeBlink(left, right);
  delay(80);
  eyeBlink(left, right);
}

void eyeSoftBlink(Adafruit_SSD1306& left, Adafruit_SSD1306& right) {
  for (int i = 0; i < 3; i++) {
    left.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    right.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    left.display(); right.display(); delay(30);
  }
  left.fillRect(30, 35, 68, 4, WHITE);
  right.fillRect(30, 35, 68, 4, WHITE);
  left.display(); right.display(); delay(80);
  for (int i = 2; i >= 0; i--) {
    left.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    right.fillRect(30, 25 + i*3, 68, 20 - i*6, WHITE);
    left.display(); right.display(); delay(30);
  }
}
