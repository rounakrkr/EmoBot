/**
 * Emo V2 — Servo Control
 * Added: servoTiltHead (curious head tilt)
 * Kept: all V1 movements
 */
#pragma once
#include <ESP32Servo.h>
#include "../config.h"

// ── Setup ────────────────────────────────────────────
void servoSetup(Servo& tilt, Servo& rotate) {
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  tilt.setPeriodHertz(50);
  rotate.setPeriodHertz(50);
  tilt.attach(PIN_SERVO_TILT,   500, 2400);
  rotate.attach(PIN_SERVO_ROTATE, 500, 2400);
  tilt.write(TILT_CENTER);
  rotate.write(ROTATE_CENTER);
  delay(300);
  Serial.println("[SERVO] Servos initialised");
}

// ── Smooth move helpers ──────────────────────────────
static void _smoothMove(Servo& s, int from, int to, int stepDelay = 20) {
  if (from == to) return;
  int step = (to > from) ? 1 : -1;
  for (int pos = from; pos != to; pos += step) { s.write(pos); delay(stepDelay); }
  s.write(to);
}

static void _smoothMoveTwo(Servo& s1, int f1, int t1,
                             Servo& s2, int f2, int t2, int stepDelay = 20) {
  int steps = max(abs(t1 - f1), abs(t2 - f2));
  if (steps == 0) return;
  for (int i = 1; i <= steps; i++) {
    float r = (float)i / steps;
    s1.write(f1 + (int)((t1 - f1) * r));
    s2.write(f2 + (int)((t2 - f2) * r));
    delay(stepDelay);
  }
}

// ── Basic positions ──────────────────────────────────
void servoCenter(Servo& tilt, Servo& rotate) {
  _smoothMove(tilt,   tilt.read(),   TILT_CENTER,   20);
  _smoothMove(rotate, rotate.read(), ROTATE_CENTER, 20);
}

void servoLookForward(Servo& tilt, Servo& rotate) {
  _smoothMoveTwo(tilt, tilt.read(), TILT_UP + 5,
                  rotate, rotate.read(), ROTATE_CENTER, 20);
}

// ── Idle ─────────────────────────────────────────────
void servoIdleLook(Servo& tilt, Servo& rotate) {
  // Only look UP or sideways — never below center
  int newTilt   = random(TILT_UP, TILT_CENTER);
  int newRotate = random(ROTATE_LEFT + 10, ROTATE_RIGHT - 10);
  _smoothMoveTwo(tilt, tilt.read(), newTilt, rotate, rotate.read(), newRotate, 25);
  delay(random(400, 900));
  _smoothMoveTwo(tilt, tilt.read(), TILT_CENTER, rotate, rotate.read(), ROTATE_CENTER, 25);
}

// Look up toward user's face
void servoLookUp(Servo& tilt) {
  _smoothMove(tilt, tilt.read(), TILT_UP, 20);
  delay(random(500, 1200));
  _smoothMove(tilt, tilt.read(), TILT_CENTER, 20);
}

void servoLookLeft(Servo& tilt, Servo& rotate) {
  _smoothMove(rotate, rotate.read(), ROTATE_LEFT, 20);
  delay(200);
  _smoothMove(rotate, rotate.read(), ROTATE_CENTER, 20);
}

void servoLookRight(Servo& tilt, Servo& rotate) {
  _smoothMove(rotate, rotate.read(), ROTATE_RIGHT, 20);
  delay(200);
  _smoothMove(rotate, rotate.read(), ROTATE_CENTER, 20);
}

// ── Listening ────────────────────────────────────────
void servoNod(Servo& tilt) {
  _smoothMove(tilt, tilt.read(), TILT_CENTER, 20);
  delay(100);
  _smoothMove(tilt, TILT_CENTER, TILT_DOWN - 10, 20);
  delay(150);
  _smoothMove(tilt, TILT_DOWN - 10, TILT_CENTER, 20);
}

// ── Happy / Excited ──────────────────────────────────
void servoEnthusiasticNod(Servo& tilt, Servo& rotate) {
  _smoothMove(tilt, tilt.read(), TILT_CENTER, 18);
  delay(100);
  for (int i = 0; i < 2; i++) {
    _smoothMove(tilt, TILT_CENTER, TILT_DOWN - 5, 15); delay(80);
    _smoothMove(tilt, TILT_DOWN - 5, TILT_UP + 5, 15); delay(80);
  }
  _smoothMove(tilt, tilt.read(), TILT_CENTER, 18);
  _smoothMove(rotate, rotate.read(), ROTATE_CENTER, 18); delay(50);
  _smoothMove(rotate, ROTATE_CENTER, ROTATE_CENTER + 10, 20); delay(200);
  _smoothMove(rotate, ROTATE_CENTER + 10, ROTATE_CENTER - 10, 20); delay(200);
  _smoothMove(rotate, ROTATE_CENTER - 10, ROTATE_CENTER, 20);
}

// ── Curious — gentle head tilt (V2 new) ─────────────
void servoTiltHead(Servo& rotate) {
  _smoothMove(rotate, rotate.read(), ROTATE_CENTER + 18, 25);
  delay(600);
  _smoothMove(rotate, ROTATE_CENTER + 18, ROTATE_CENTER, 25);
}

// ── Neutral ──────────────────────────────────────────
void servoGentleSway(Servo& rotate) {
  _smoothMove(rotate, rotate.read(), ROTATE_CENTER, 25); delay(50);
  _smoothMove(rotate, ROTATE_CENTER, ROTATE_CENTER + 10, 30); delay(200);
  _smoothMove(rotate, ROTATE_CENTER + 10, ROTATE_CENTER - 10, 30); delay(200);
  _smoothMove(rotate, ROTATE_CENTER - 10, ROTATE_CENTER, 30);
}

// ── Sad ──────────────────────────────────────────────
void servoDroop(Servo& tilt) {
  _smoothMove(tilt, tilt.read(), TILT_CENTER, 25); delay(50);
  _smoothMove(tilt, TILT_CENTER, TILT_DROOP, 30); delay(600);
  _smoothMove(tilt, TILT_DROOP, TILT_CENTER, 25);
}
