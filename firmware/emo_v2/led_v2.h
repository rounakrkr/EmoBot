/**
 * Emo V2 — LED Control
 * New emotion colors added: curious, angry, excited, thinking, speaking
 */
#pragma once
#include "../config.h"

// ── Color struct ────────────────────────────────────
struct RGBColor { uint8_t r, g, b; };

// ── Emotion colors ──────────────────────────────────
constexpr RGBColor LED_BOOT     = {20,  20,  20};   // dim white
constexpr RGBColor LED_IDLE     = {5,   5,   5};    // very dim white
constexpr RGBColor LED_LISTENING= {0,   80,  255};  // blue pulse
constexpr RGBColor LED_THINKING = {80,  0,   180};  // purple breathe
constexpr RGBColor LED_SPEAKING = {255, 255, 255};  // bright white
constexpr RGBColor LED_HAPPY    = {255, 180, 0};    // warm yellow
constexpr RGBColor LED_SAD      = {0,   30,  120};  // deep blue
constexpr RGBColor LED_CURIOUS  = {180, 0,   255};  // violet
constexpr RGBColor LED_ANGRY    = {255, 10,  0};    // red
constexpr RGBColor LED_EXCITED  = {255, 220, 0};    // bright yellow
constexpr RGBColor LED_NEUTRAL  = {40,  40,  40};   // dim white

// ── State ────────────────────────────────────────────
static RGBColor _currentColor = {0, 0, 0};
static float    _pulsePhase   = 0.0f;
static bool     _pulsing      = false;

// ── API ──────────────────────────────────────────────

void ledSetup() {
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_LED_R, 0);
  ledcWrite(PIN_LED_G, 0);
  ledcWrite(PIN_LED_B, 0);
  Serial.println("[LED] RGB initialised");
}

void ledSet(RGBColor c) {
  _currentColor = c;
  _pulsing = false;
  ledcWrite(PIN_LED_R, c.r);
  ledcWrite(PIN_LED_G, c.g);
  ledcWrite(PIN_LED_B, c.b);
}

void ledStartPulse() {
  _pulsing    = true;
  _pulsePhase = 0.0f;
}

void ledPulse() {
  if (!_pulsing) return;
  _pulsePhase += 0.05f;
  if (_pulsePhase > TWO_PI) _pulsePhase -= TWO_PI;
  float brightness = 0.4f + 0.6f * sin(_pulsePhase);
  ledcWrite(PIN_LED_R, (uint8_t)(_currentColor.r * brightness));
  ledcWrite(PIN_LED_G, (uint8_t)(_currentColor.g * brightness));
  ledcWrite(PIN_LED_B, (uint8_t)(_currentColor.b * brightness));
  delay(15);
}

void ledFlash(RGBColor c, int times = 2, int ms = 80) {
  RGBColor prev = _currentColor;
  for (int i = 0; i < times; i++) {
    ledSet(c);
    delay(ms);
    ledcWrite(PIN_LED_R, 0); ledcWrite(PIN_LED_G, 0); ledcWrite(PIN_LED_B, 0);
    delay(ms / 2);
  }
  ledSet(prev);
}
