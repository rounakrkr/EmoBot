/**
 * Emo Robot V2 — Main Firmware
 *
 * Architecture:
 *   ESP32 ←→ WebSocket ←→ Oracle Cloud Backend
 *
 * State Machine:
 *   IDLE → LISTENING → THINKING → SPEAKING → IDLE
 *   (with Attention Window: no wake word needed for 20s after response)
 *
 * Hardware:
 *   - 2x SSD1306 OLED (I2C, different addresses)
 *   - 2x SG90 Servos
 *   - RGB LED
 *   - Bluetooth Speaker (A2DP)
 *   - [Future] INMP441 mic + MAX98357A amp
 *
 * Libraries required:
 *   - WebSockets by Markus Sattler (Links2004)
 *   - ArduinoJson by Benoit Blanchon
 *   - arduino-libhelix by Phil Schatzmann (MP3 decode)
 *   - Adafruit SSD1306 + GFX
 *   - ESP32Servo
 */

#include <WiFi.h>
#include <esp_coexist.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "../config.h"   // All settings
#include "eyes_v2.h"
#include "led_v2.h"
#include "servos_v2.h"
#include "audio_player.h"

// ═══════════════════════════════════════════════════
//  STATES
// ═══════════════════════════════════════════════════

enum BotState {
  STATE_IDLE,
  STATE_LISTENING,
  STATE_THINKING,
  STATE_SPEAKING,
  // Emotion states (held after speaking until next interaction)
  STATE_HAPPY,
  STATE_SAD,
  STATE_CURIOUS,
  STATE_ANGRY,
  STATE_EXCITED,
  STATE_NEUTRAL
};

const char* stateName(BotState s) {
  switch (s) {
    case STATE_IDLE:      return "IDLE";
    case STATE_LISTENING: return "LISTENING";
    case STATE_THINKING:  return "THINKING";
    case STATE_SPEAKING:  return "SPEAKING";
    case STATE_HAPPY:     return "HAPPY";
    case STATE_SAD:       return "SAD";
    case STATE_CURIOUS:   return "CURIOUS";
    case STATE_ANGRY:     return "ANGRY";
    case STATE_EXCITED:   return "EXCITED";
    case STATE_NEUTRAL:   return "NEUTRAL";
    default:              return "UNKNOWN";
  }
}

// ═══════════════════════════════════════════════════
//  GLOBALS
// ═══════════════════════════════════════════════════

Adafruit_SSD1306 leftEye (OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 rightEye(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Servo servoTilt;
Servo servoRotate;

WebSocketsClient ws;

BotState currentState = STATE_IDLE;

// Attention Window
unsigned long attentionStart    = 0;
bool          inAttentionWindow = false;
unsigned long attentionDuration = ATTENTION_TIMEOUT_MS;

// Animation timing
unsigned long lastIdleAnim   = 0;
unsigned long lastTickAction = 0;

// WebSocket keepalive
unsigned long lastPing = 0;

// WiFi reconnect
unsigned long lastWifiCheck = 0;

// Gesture deferred until speech finishes (prevents blocking audio stream)
static bool          _gesturePending         = false;
static BotState      _pendingGestureState    = STATE_NEUTRAL;
static bool          _audioStartedForGesture = false;
static unsigned long _gestureSetTime         = 0;

// ═══════════════════════════════════════════════════
//  STATE MACHINE
// ═══════════════════════════════════════════════════

void enterState(BotState newState) {
  Serial.printf("[STATE] %s → %s\n", stateName(currentState), stateName(newState));
  currentState   = newState;
  lastTickAction = millis();

  switch (newState) {
    case STATE_IDLE:
      ledSet(LED_IDLE);
      eyeShowExpression(leftEye, rightEye, EYE_NORMAL);
      servoCenter(servoTilt, servoRotate);
      inAttentionWindow = false;
      break;

    case STATE_LISTENING:
      ledSet(LED_LISTENING);
      ledStartPulse();
      eyeShowExpression(leftEye, rightEye, EYE_ATTENTIVE);
      servoLookForward(servoTilt, servoRotate);
      break;

    case STATE_THINKING:
      ledSet(LED_THINKING);
      eyeShowExpression(leftEye, rightEye, EYE_THINKING);
      servoCenter(servoTilt, servoRotate);
      break;

    case STATE_SPEAKING:
      ledSet(LED_SPEAKING);
      eyeShowExpression(leftEye, rightEye, EYE_NORMAL);
      break;

    case STATE_HAPPY:
      ledSet(LED_HAPPY);
      eyeShowExpression(leftEye, rightEye, EYE_HAPPY);
      _pendingGestureState    = STATE_HAPPY;
      _gesturePending         = true;
      _audioStartedForGesture = false;
      _gestureSetTime         = millis();
      break;

    case STATE_SAD:
      ledSet(LED_SAD);
      eyeShowExpression(leftEye, rightEye, EYE_SAD);
      _pendingGestureState    = STATE_SAD;
      _gesturePending         = true;
      _audioStartedForGesture = false;
      _gestureSetTime         = millis();
      break;

    case STATE_CURIOUS:
      ledSet(LED_CURIOUS);
      eyeShowExpression(leftEye, rightEye, EYE_CURIOUS);
      _pendingGestureState    = STATE_CURIOUS;
      _gesturePending         = true;
      _audioStartedForGesture = false;
      _gestureSetTime         = millis();
      break;

    case STATE_ANGRY:
      ledSet(LED_ANGRY);
      eyeShowExpression(leftEye, rightEye, EYE_ANGRY);
      _pendingGestureState    = STATE_ANGRY;
      _gesturePending         = true;
      _audioStartedForGesture = false;
      _gestureSetTime         = millis();
      break;

    case STATE_EXCITED:
      ledSet(LED_EXCITED);
      eyeShowExpression(leftEye, rightEye, EYE_HAPPY);
      _pendingGestureState    = STATE_EXCITED;
      _gesturePending         = true;
      _audioStartedForGesture = false;
      _gestureSetTime         = millis();
      break;

    case STATE_NEUTRAL:
      ledSet(LED_NEUTRAL);
      eyeShowExpression(leftEye, rightEye, EYE_NORMAL);
      _pendingGestureState    = STATE_NEUTRAL;
      _gesturePending         = true;
      _audioStartedForGesture = false;
      _gestureSetTime         = millis();
      break;
  }
}

// Map emotion string from backend → BotState
void applyEmotion(const String& emotion) {
  if      (emotion == "happy")   enterState(STATE_HAPPY);
  else if (emotion == "sad")     enterState(STATE_SAD);
  else if (emotion == "curious") enterState(STATE_CURIOUS);
  else if (emotion == "angry")   enterState(STATE_ANGRY);
  else if (emotion == "excited") enterState(STATE_EXCITED);
  else                           enterState(STATE_NEUTRAL);
}

// ═══════════════════════════════════════════════════
//  ATTENTION WINDOW
// ═══════════════════════════════════════════════════

void startAttentionWindow(unsigned long durationMs) {
  attentionStart    = millis();
  attentionDuration = durationMs;
  inAttentionWindow = true;
  Serial.printf("[ATTN] Window started (%lums)\n", durationMs);
}

bool checkAttentionWindow() {
  if (!inAttentionWindow) return false;
  if (millis() - attentionStart >= attentionDuration) {
    inAttentionWindow = false;
    Serial.println("[ATTN] Window expired → IDLE");
    enterState(STATE_IDLE);
    return false;
  }
  return true;
}

// ═══════════════════════════════════════════════════
//  WEBSOCKET
// ═══════════════════════════════════════════════════

void sendWs(const char* type) {
  StaticJsonDocument<128> doc;
  doc["type"] = type;
  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {

    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected — will retry...");
      break;

    case WStype_CONNECTED:
      Serial.println("[WS] Connected to Emo backend!");
      sendWs("ping");
      break;

    // ── Binary: raw PCM chunk from backend ─────────────
    // Protocol: audio_start → [BIN chunks] → audio_end
    case WStype_BIN:
      audioPlayer.onPCMChunk(payload, length);
      break;

    case WStype_TEXT: {
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) { Serial.println("[WS] JSON parse error"); return; }

      const char* msgType = doc["type"] | "unknown";
      Serial.printf("[WS] ← %s\n", msgType);

      if (strcmp(msgType, "connected") == 0) {
        Serial.println("[WS] Backend ready!");
      }
      else if (strcmp(msgType, "pong") == 0) {
        // keepalive ack
      }
      else if (strcmp(msgType, "state") == 0) {
        const char* state = doc["state"] | "idle";
        if      (strcmp(state, "listening") == 0) enterState(STATE_LISTENING);
        else if (strcmp(state, "thinking")  == 0) enterState(STATE_THINKING);
        else if (strcmp(state, "speaking")  == 0) enterState(STATE_SPEAKING);
        else if (strcmp(state, "idle")      == 0) enterState(STATE_IDLE);
      }
      else if (strcmp(msgType, "response") == 0) {
        String emotion   = doc["emotion"]      | "neutral";
        bool expectReply = doc["expect_reply"] | false;
        applyEmotion(emotion);
        startAttentionWindow(expectReply ? ATTENTION_EXTENDED_MS : ATTENTION_TIMEOUT_MS);
      }
      // ── Audio stream lifecycle ─────────────────────────
      else if (strcmp(msgType, "audio_start") == 0) {
        int totalBytes = doc["total_bytes"] | 0;
        Serial.printf("[WS] Audio stream → %d bytes incoming\n", totalBytes);
        audioPlayer.onAudioStart();
        _audioStartedForGesture = true;
      }
      else if (strcmp(msgType, "audio_end") == 0) {
        audioPlayer.onAudioEnd();
      }
      break;
    }

    default: break;
  }
}

void connectWebSocket() {
  Serial.printf("[WS] Connecting to ws://%s:%d%s\n",
                BACKEND_HOST, BACKEND_PORT, BACKEND_WS_PATH);
  ws.begin(BACKEND_HOST, BACKEND_PORT, BACKEND_WS_PATH);
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);
  ws.enableHeartbeat(15000, 8000, 3);
}

// ═══════════════════════════════════════════════════
//  IDLE ANIMATIONS (ESP32 behaves like a living creature)
// ═══════════════════════════════════════════════════

void tickIdle() {
  unsigned long now = millis();
  static unsigned long nextAnim = 4000;

  if (now - lastIdleAnim > nextAnim) {
    lastIdleAnim = now;
    nextAnim = random(3000, 8000);

    switch (random(7)) {
      case 0: eyeBlink(leftEye, rightEye);              break;
      case 1: eyeDoubleBlink(leftEye, rightEye);        break;
      case 2: servoIdleLook(servoTilt, servoRotate);    break;
      case 3: servoLookLeft(servoTilt, servoRotate);    break;
      case 4: servoLookRight(servoTilt, servoRotate);   break;
      case 5: servoLookUp(servoTilt);                   break;  // look up at user
      case 6:
        eyeBlink(leftEye, rightEye);
        servoLookUp(servoTilt);
        break;
    }
    servoCenter(servoTilt, servoRotate);
  }
}

void tickListening() {
  ledPulse();
  unsigned long now = millis();
  static unsigned long lastNod = 0;
  if (now - lastNod > 5000) { lastNod = now; servoNod(servoTilt); }
}

void tickThinking() {
  // Slow LED breathe handled in ledPulse variant
  unsigned long now = millis();
  if (now - lastTickAction > 3000) {
    lastTickAction = now;
    // Subtle eye shift — looking up/thinking
    eyeShowExpression(leftEye, rightEye, EYE_THINKING);
  }
}

void tickEmotionState() {
  // If a gesture is pending, audio has started, and audio has now finished (with 7s safety timeout):
  if (_gesturePending && ((_audioStartedForGesture && !audioPlayer.isPlaying()) || (millis() - _gestureSetTime > 7000))) {
    _gesturePending = false;
    _audioStartedForGesture = false;
    switch (_pendingGestureState) {
      case STATE_HAPPY:
      case STATE_EXCITED: servoEnthusiasticNod(servoTilt, servoRotate); break;
      case STATE_SAD:     servoDroop(servoTilt);                        break;
      case STATE_CURIOUS: servoTiltHead(servoRotate);                   break;
      case STATE_ANGRY:
      case STATE_NEUTRAL: servoCenter(servoTilt, servoRotate);          break;
      default: break;
    }
  }

  // Occasional blink to keep robot feeling alive
  unsigned long now = millis();
  static unsigned long nextBlink = 4000;
  if (now - lastTickAction > nextBlink) {
    lastTickAction = now;
    nextBlink = random(3000, 7000);
    eyeBlink(leftEye, rightEye);
    // Restore expression after blink
    switch (currentState) {
      case STATE_HAPPY:
      case STATE_EXCITED:  eyeShowExpression(leftEye, rightEye, EYE_HAPPY);   break;
      case STATE_SAD:      eyeShowExpression(leftEye, rightEye, EYE_SAD);     break;
      case STATE_CURIOUS:  eyeShowExpression(leftEye, rightEye, EYE_CURIOUS); break;
      case STATE_ANGRY:    eyeShowExpression(leftEye, rightEye, EYE_ANGRY);   break;
      default:             eyeShowExpression(leftEye, rightEye, EYE_NORMAL);  break;
    }
  }
}

// ═══════════════════════════════════════════════════
//  WAKE WORD SIMULATION
//  [V2 placeholder — replace with Porcupine later]
//  Press BOOT button (GPIO 0) to simulate wake word
// ═══════════════════════════════════════════════════

#define WAKE_BUTTON_PIN 0

void checkWakeWord() {
  static bool lastBtn = HIGH;
  bool btn = digitalRead(WAKE_BUTTON_PIN);

  if (btn == LOW && lastBtn == HIGH) {
    lastBtn = LOW;
    delay(50); // debounce

    if (currentState == STATE_IDLE || inAttentionWindow) {
      Serial.println("[WAKE] Wake word detected (button)!");
      // Notify backend
      StaticJsonDocument<128> doc;
      doc["type"] = "wake_detected";
      String out;
      serializeJson(doc, out);
      ws.sendTXT(out);
      enterState(STATE_LISTENING);
    }
  }
  if (btn == HIGH) lastBtn = HIGH;
}

// ═══════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════

void setup() {
  Serial.begin(DEBUG_BAUD_RATE);
  delay(500);
  Serial.println("\n🤖 Emo Robot V2 starting...");

  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);

  // Hardware init
  ledSetup();
  ledSet(LED_BOOT);
  servoSetup(servoTilt, servoRotate);

  // ── Servo self-test (confirm wiring) ───────────────
  Serial.println("[SERVO] Self-test: tilt up/down...");
  servoTilt.write(TILT_UP);    delay(600);
  servoTilt.write(TILT_DOWN);  delay(600);
  servoTilt.write(TILT_CENTER);delay(400);
  Serial.println("[SERVO] Self-test: rotate left/right...");
  servoRotate.write(ROTATE_LEFT);  delay(600);
  servoRotate.write(ROTATE_RIGHT); delay(600);
  servoRotate.write(ROTATE_CENTER);delay(400);
  Serial.println("[SERVO] Self-test done!");
  // ────────────────────────────────────────────────────

  eyeSetup(leftEye, rightEye);

  // Bluetooth Audio — init BEFORE WiFi for stable coexistence
  audioPlayer.begin();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting to ");
  Serial.print(WIFI_SSID);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Connected! IP: " + WiFi.localIP().toString());
    WiFi.setSleep(false);
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    Serial.printf("[MEM] Free heap after WiFi+BT: %u bytes | Min heap: %u bytes\n",
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
  } else {
    Serial.println("\n[WiFi] ✗ Failed — running offline");
  }

  // WebSocket
  connectWebSocket();

  // Ready
  enterState(STATE_IDLE);
  Serial.println("🤖 Emo ready! Press BOOT button to simulate wake word.");
}

// ═══════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════

void loop() {
  unsigned long now = millis();

  // ── Serial Monitor input (testing without mic) ──────
  // Type anything in Serial Monitor → Emo responds via BT speaker
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0 && ws.isConnected()) {
      if (!audioPlayer.isConnected()) {
        Serial.println("[AUDIO] ⚠️ Warning: BT speaker 'Mini boost 4' is not connected yet!");
      }
      StaticJsonDocument<256> doc;
      doc["type"] = "text_input";
      doc["text"] = line;
      String out;
      serializeJson(doc, out);
      ws.sendTXT(out);
      Serial.println("[YOU] " + line);
      enterState(STATE_THINKING);
    }
  }

  // WebSocket
  ws.loop();

  // Audio playback
  audioPlayer.loop();

  // When speaking finishes → check attention window
  if (currentState == STATE_SPEAKING && !audioPlayer.isPlaying()) {
    // Audio done — stay in emotion state, attention window handles timeout
  }

  // Keepalive ping every 15s
  if (now - lastPing > 15000) {
    lastPing = now;
    if (ws.isConnected()) sendWs("ping");
  }

  // WiFi watchdog
  if (now - lastWifiCheck > 30000) {
    lastWifiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Reconnecting...");
      WiFi.reconnect();
    }
  }

  // Wake word check
  checkWakeWord();

  // Attention window
  if (currentState != STATE_IDLE &&
      currentState != STATE_LISTENING &&
      currentState != STATE_THINKING) {
    checkAttentionWindow();
  }

  // Per-state tick
  switch (currentState) {
    case STATE_IDLE:      if (!audioPlayer.isPlaying()) tickIdle(); break;
    case STATE_LISTENING: tickListening();     break;
    case STATE_THINKING:  tickThinking();      break;
    case STATE_HAPPY:
    case STATE_SAD:
    case STATE_CURIOUS:
    case STATE_ANGRY:
    case STATE_EXCITED:
    case STATE_NEUTRAL:
    case STATE_SPEAKING:
      if (!audioPlayer.isPlaying()) tickEmotionState();
      break;
  }
}
