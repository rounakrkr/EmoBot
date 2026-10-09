#pragma once
#include "config.h"

static RGBColor _currentColor = {0, 0, 0};
static float    _pulsePhase   = 0.0f;
static bool     _pulsing      = false;

void ledSetup() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  // New API in ESP32 core 3.x
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_B, PWM_FREQ, PWM_RES);

  ledcWrite(PIN_LED_R, 0);
  ledcWrite(PIN_LED_G, 0);
  ledcWrite(PIN_LED_B, 0);

  Serial.println("[LED] RGB LED initialised");
}

void ledSet(RGBColor c) {
  _currentColor = c;
  _pulsing      = false;
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

  float brightness = 0.5f + 0.5f * sin(_pulsePhase);

  ledcWrite(PIN_LED_R, (uint8_t)(_currentColor.r * brightness));
  ledcWrite(PIN_LED_G, (uint8_t)(_currentColor.g * brightness));
  ledcWrite(PIN_LED_B, (uint8_t)(_currentColor.b * brightness));

  delay(20);
}

void ledFlash(RGBColor c, int times = 2, int ms = 80) {
  RGBColor prev = _currentColor;
  for (int i = 0; i < times; i++) {
    ledSet(c);
    delay(ms);
    ledcWrite(PIN_LED_R, 0);
    ledcWrite(PIN_LED_G, 0);
    ledcWrite(PIN_LED_B, 0);
    delay(ms / 2);
  }
  ledSet(prev);
}