/*
 * Emotion Bot - ESP32 Firmware
 * 
 * Hardware:
 *   - 2x SG90 Servos (tilt + rotate) on GPIO pins
 *   - 2x SSD1306 OLED displays (shared I2C, different addresses)
 *   - 1x RGB LED (indicator)
 *   - WiFi for HTTP server
 * 
 * States: IDLE, LISTENING, POSITIVE, NEUTRAL, NEGATIVE
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "led.h"
#include "eyes.h"
#include "servos.h"

// ── Global State ───────────────────────────────────────────────
WebServer server(80);

BotState      currentState   = STATE_IDLE;
BotState      pendingState   = STATE_IDLE;
bool          stateChanged   = false;
unsigned long stateEnteredAt = 0;
unsigned long lastIdleAction = 0;

// OLED displays — both on same I2C bus, different addresses
Adafruit_SSD1306 leftEye (OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 rightEye(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Servos
Servo servoTilt;
Servo servoRotate;

// ── Forward declarations ───────────────────────────────────────
void enterState(BotState newState);
void tickIdle();
void tickListening();
void tickPositive();
void tickNeutral();
void tickNegative();
void handleIdle();
void handleListening();
void handlePositive();
void handleNeutral();
void handleNegative();
void handleStatus();
void scheduleIdleAfterGrace();
void cancelIdleGrace();
void checkIdleGrace();
const char* stateName(BotState s);

// ── Setup ──────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] Emotion Bot starting...");

  // LED first
  ledSetup();
  ledSet(LED_BOOT);

  // Servos after LED (avoids PWM timer conflict)
  servoSetup(servoTilt, servoRotate);

  // OLEDs
  eyeSetup(leftEye, rightEye);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WiFi] Connected! IP: ");
  Serial.println(WiFi.localIP());

  // HTTP Routes — POST for real use
  server.on("/idle",      HTTP_POST, handleIdle);
  server.on("/listening", HTTP_POST, handleListening);
  server.on("/positive",  HTTP_POST, handlePositive);
  server.on("/neutral",   HTTP_POST, handleNeutral);
  server.on("/negative",  HTTP_POST, handleNegative);
  server.on("/status",    HTTP_GET,  handleStatus);

  // GET routes for browser/phone testing
  server.on("/idle",      HTTP_GET, handleIdle);
  server.on("/listening", HTTP_GET, handleListening);
  server.on("/positive",  HTTP_GET, handlePositive);
  server.on("/neutral",   HTTP_GET, handleNeutral);
  server.on("/negative",  HTTP_GET, handleNegative);

  // Test page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<h2>Emotion Bot</h2>"
      "<a href='/idle'><button>IDLE</button></a> "
      "<a href='/listening'><button>LISTENING</button></a> "
      "<a href='/positive'><button>POSITIVE</button></a> "
      "<a href='/neutral'><button>NEUTRAL</button></a> "
      "<a href='/negative'><button>NEGATIVE</button></a>"
    );
  });

  server.begin();
  Serial.println("[HTTP] Server started");

  enterState(STATE_IDLE);
  Serial.println("[BOOT] Ready!");
}

// ── Main Loop ──────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkIdleGrace();

  if (stateChanged) {
    stateChanged = false;
    enterState(pendingState);
  }

  switch (currentState) {
    case STATE_IDLE:      tickIdle();      break;
    case STATE_LISTENING: tickListening(); break;
    case STATE_POSITIVE:  tickPositive();  break;
    case STATE_NEUTRAL:   tickNeutral();   break;
    case STATE_NEGATIVE:  tickNegative();  break;
  }
}

// ── State Machine ──────────────────────────────────────────────
void enterState(BotState newState) {
  currentState   = newState;
  stateEnteredAt = millis();
  lastIdleAction = millis();

  Serial.printf("[STATE] -> %s\n", stateName(newState));

  switch (newState) {
    case STATE_IDLE:
      ledSet(LED_IDLE);
      eyeShowExpression(leftEye, rightEye, EYE_NORMAL);
      servoCenter(servoTilt, servoRotate);
      break;

    case STATE_LISTENING:
      ledSet(LED_LISTENING);
      ledStartPulse();
      eyeShowExpression(leftEye, rightEye, EYE_ATTENTIVE);
      servoCenter(servoTilt, servoRotate);
      servoLookForward(servoTilt, servoRotate);
      break;

    case STATE_POSITIVE:
      ledSet(LED_POSITIVE);
      eyeShowExpression(leftEye, rightEye, EYE_HAPPY);
      servoCenter(servoTilt, servoRotate);
      break;

    case STATE_NEUTRAL:
      ledSet(LED_NEUTRAL);
      eyeShowExpression(leftEye, rightEye, EYE_NORMAL);
      servoCenter(servoTilt, servoRotate);
      break;

    case STATE_NEGATIVE:
      ledSet(LED_NEGATIVE);
      eyeShowExpression(leftEye, rightEye, EYE_SAD);
      servoCenter(servoTilt, servoRotate);
      break;
  }
}

// ── Tick Functions ─────────────────────────────────────────────

void tickIdle() {
  unsigned long now = millis();
  static unsigned long nextActionDelay = 4000;

  if (now - lastIdleAction > nextActionDelay) {
    lastIdleAction  = now;
    nextActionDelay = random(3000, 8000);

    switch (random(6)) {  // 4 → 6 (more variety)
      case 0: eyeBlink(leftEye, rightEye);                    break;
      case 1: eyeDoubleBlink(leftEye, rightEye);              break;
      case 2: servoIdleLook(servoTilt, servoRotate);          break;
      case 3: servoLookLeft(servoTilt, servoRotate);          break;  // naya
      case 4: servoLookRight(servoTilt, servoRotate);         break;  // naya
      case 5:
        eyeBlink(leftEye, rightEye);
        servoIdleLook(servoTilt, servoRotate); 
        break;
    }
    servoCenter(servoTilt, servoRotate);
  }
}

void tickListening() {
  ledPulse();

  static unsigned long lastNod   = 0;
  static bool          firstTick = true;

  if (firstTick) {
    if (millis() - stateEnteredAt > 2000) {
      firstTick = false;
      lastNod   = millis();
    }
    return;
  }

  if (millis() - lastNod > 5000) {
    lastNod = millis();
    servoNod(servoTilt);
  }
}

void tickPositive() {
  static unsigned long lastAnim  = 0;
  static int           animStep  = 0;
  static BotState      lastState = STATE_IDLE;

  if (lastState != STATE_POSITIVE) {
    animStep  = 0;
    lastAnim  = millis();
    lastState = STATE_POSITIVE;
  }

  if (millis() - lastAnim > 1200) {
    lastAnim = millis();
    animStep = (animStep + 1) % 3;

    switch (animStep) {
      case 0: eyeShowExpression(leftEye, rightEye, EYE_HAPPY);       break;
      case 1: eyeShowExpression(leftEye, rightEye, EYE_HAPPY_BLINK); break;
      case 2:
        eyeShowExpression(leftEye, rightEye, EYE_HAPPY);
        servoEnthusiasticNod(servoTilt, servoRotate);
        break;
    }
  }
}

void tickNeutral() {
  static unsigned long lastAnim     = 0;
  static unsigned long animInterval = 3000;

  if (millis() - lastAnim > animInterval) {
    lastAnim     = millis();
    animInterval = random(3000, 5000);

    switch (random(3)) {
      case 0: eyeBlink(leftEye, rightEye);  break;
      case 1: servoGentleSway(servoRotate); break;
      case 2: /* intentional pause */       break;
    }
  }
}

void tickNegative() {
  static unsigned long lastAnim     = 0;
  static unsigned long animInterval = 3000;

  if (millis() - lastAnim > animInterval) {
    lastAnim     = millis();
    animInterval = random(2500, 5000);

    switch (random(3)) {
      case 0: eyeSoftBlink(leftEye, rightEye);               break;
      case 1: servoDroop(servoTilt);                         break;
      case 2: eyeShowExpression(leftEye, rightEye, EYE_SAD); break;
    }
  }
}

// ── HTTP Handlers ──────────────────────────────────────────────

void handleIdle() {
  Serial.println("[HTTP] /idle received");
  scheduleIdleAfterGrace();
  server.send(200, "text/plain", "OK");
}

void handleListening() {
  Serial.println("[HTTP] /listening received");
  cancelIdleGrace();
  pendingState = STATE_LISTENING;
  stateChanged = true;
  server.send(200, "text/plain", "OK");
}

void handlePositive() {
  Serial.println("[HTTP] /positive received");
  cancelIdleGrace();
  pendingState = STATE_POSITIVE;
  stateChanged = true;
  server.send(200, "text/plain", "OK");
}

void handleNeutral() {
  Serial.println("[HTTP] /neutral received");
  cancelIdleGrace();
  pendingState = STATE_NEUTRAL;
  stateChanged = true;
  server.send(200, "text/plain", "OK");
}

void handleNegative() {
  Serial.println("[HTTP] /negative received");
  cancelIdleGrace();
  pendingState = STATE_NEGATIVE;
  stateChanged = true;
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String json = "{\"state\":\"";
  json += stateName(currentState);
  json += "\",\"uptime\":";
  json += millis();
  json += "}";
  server.send(200, "application/json", json);
}

// ── Idle Grace Period ──────────────────────────────────────────

bool          idleGracePending = false;
unsigned long idleGraceStart   = 0;
const unsigned long IDLE_GRACE_MS = 2500;

void scheduleIdleAfterGrace() {
  idleGracePending = true;
  idleGraceStart   = millis();
}

void cancelIdleGrace() {
  idleGracePending = false;
}

void checkIdleGrace() {
  if (idleGracePending && (millis() - idleGraceStart >= IDLE_GRACE_MS)) {
    idleGracePending = false;
    pendingState     = STATE_IDLE;
    stateChanged     = true;
  }
}

// ── Helpers ────────────────────────────────────────────────────
const char* stateName(BotState s) {
  switch (s) {
    case STATE_IDLE:      return "idle";
    case STATE_LISTENING: return "listening";
    case STATE_POSITIVE:  return "positive";
    case STATE_NEUTRAL:   return "neutral";
    case STATE_NEGATIVE:  return "negative";
    default:              return "unknown";
  }
}