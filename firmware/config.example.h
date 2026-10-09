/**
 * Emo Robot — Central Configuration File
 *
 * ⚠️  Copy this file to config.h and fill in real values.
 *     config.h is git-ignored — NEVER commit real credentials!
 *
 * Change your WiFi, backend, audio, and behavior
 * settings here in one place.
 */

#pragma once

// ── DEVELOPMENT: Personal Hotspot (WPA2-Personal) ─────────────────────────
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ── PRODUCTION: WPA2-Enterprise (if your network needs it) ─
// #define WIFI_SSID              "YOUR_ENTERPRISE_SSID"
// #define WIFI_EAP_ANONYMOUS_ID  "YOUR_ANONYMOUS_ID"
// #define WIFI_EAP_IDENTITY      "YOUR_IDENTITY"
// #define WIFI_EAP_PASSWORD      "YOUR_ENTERPRISE_PASSWORD"
// #define WIFI_ENTERPRISE        true

// ═══════════════════════════════════════════════════
//  SECTION 2 — Cloud Backend (Oracle)
// ═══════════════════════════════════════════════════

#define BACKEND_HOST        "YOUR_SERVER_IP"
#define BACKEND_PORT        8001
#define BACKEND_WS_PATH     "/ws"

// ═══════════════════════════════════════════════════
//  SECTION 3 — Bluetooth Speaker
// ═══════════════════════════════════════════════════

#define BT_SPEAKER_NAME     "Mini boost 4"

// ═══════════════════════════════════════════════════
//  SECTION 4 — Wake Word
// ═══════════════════════════════════════════════════

// Porcupine wake word sensitivity: 0.0 (strict) → 1.0 (very sensitive)
// Lower = fewer false positives but may miss some "Hey Emo"s
// Higher = more responsive but may false-trigger on similar sounds
// Recommended: 0.6 to 0.75
#define WAKE_WORD_SENSITIVITY  0.65f

// ═══════════════════════════════════════════════════
//  SECTION 5 — Attention Window
// ═══════════════════════════════════════════════════

// How long Emo stays "alert" after responding (milliseconds)
// If user speaks within this window → no wake word needed
// If silence → Emo goes back to sleep
#define ATTENTION_TIMEOUT_MS   20000   // 20 seconds

// Extra time given when LLM signals expect_reply = true
#define ATTENTION_EXTENDED_MS  35000   // 35 seconds

// ═══════════════════════════════════════════════════
//  SECTION 6 — Audio Input (INMP441 — future)
// ═══════════════════════════════════════════════════

#define MIC_SAMPLE_RATE     16000    // Hz (Groq Whisper expects 16kHz)
#define MIC_CHANNELS        1        // Mono
#define VAD_ENERGY_THRESHOLD 500     // Adjust based on environment noise

// ═══════════════════════════════════════════════════
//  SECTION 7 — Debug
// ═══════════════════════════════════════════════════

#define DEBUG_SERIAL        true     // Print logs to Serial Monitor
#define DEBUG_BAUD_RATE     115200

// ═══════════════════════════════════════════════════
//  SECTION 8 — OLED Displays
// ═══════════════════════════════════════════════════

#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_RESET   -1     // share Arduino reset pin

// Left eye:  default I2C bus → SDA=GPIO21, SCL=GPIO22
// Right eye: same bus, different I2C address
#define LEFT_EYE_ADDR   0x3C
#define RIGHT_EYE_ADDR  0x3D  // change to 0x3C if both eyes are same address

// ═══════════════════════════════════════════════════
//  SECTION 9 — Servos
// ═══════════════════════════════════════════════════

#define PIN_SERVO_TILT    16   // up / down
#define PIN_SERVO_ROTATE  17   // left / right

#define TILT_CENTER    75    // raised — faces user, not floor
#define TILT_UP        58    // looking up
#define TILT_DOWN     110    // only for sad/droop
#define TILT_DROOP    115    // max droop (sad)

#define ROTATE_CENTER  90
#define ROTATE_LEFT    60
#define ROTATE_RIGHT  120

// ═══════════════════════════════════════════════════
//  SECTION 10 — RGB LED (Common Cathode)
// ═══════════════════════════════════════════════════

#define PIN_LED_R  2
#define PIN_LED_G  5
#define PIN_LED_B  4

#define PWM_FREQ  5000   // Hz
#define PWM_RES   8      // 8-bit → values 0-255
