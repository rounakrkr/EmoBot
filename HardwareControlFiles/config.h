#pragma once

// ── WiFi ───────────────────────────────────────────────────────
#define WIFI_SSID     "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"

// ── OLED Displays ──────────────────────────────────────────────
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_RESET  -1   // share Arduino reset pin

// Left eye:  default I2C  → SDA=GPIO21, SCL=GPIO22, addr 0x3C
// Right eye: Wire1 I2C    → SDA=GPIO25, SCL=GPIO26, addr 0x3C
#define I2C1_SDA 25
#define I2C1_SCL 26
#define LEFT_EYE_ADDR  0x3C
#define RIGHT_EYE_ADDR 0x3D   // change to 0x3D if both on same bus

// ── Servos ─────────────────────────────────────────────────────
#define PIN_SERVO_TILT   16   // up / down
#define PIN_SERVO_ROTATE 17   // left / right

// Angle limits (SG90 is 0-180 but mechanical limits may be tighter)
#define TILT_CENTER    90
#define TILT_UP        70
#define TILT_DOWN     110
#define TILT_DROOP    120

#define ROTATE_CENTER  90
#define ROTATE_LEFT    60
#define ROTATE_RIGHT  120

// ── RGB LED ────────────────────────────────────────────────────
// Common-cathode RGB LED (or use a NeoPixel on a single pin)
#define PIN_LED_R  2
#define PIN_LED_B  4
#define PIN_LED_G  5

// PWM channels
#define PWM_CH_R 0
#define PWM_CH_G 1
#define PWM_CH_B 2
#define PWM_FREQ 5000
#define PWM_RES  8   // 8-bit → 0-255

// ── Bot States ─────────────────────────────────────────────────
enum BotState {
  STATE_IDLE,
  STATE_LISTENING,
  STATE_POSITIVE,
  STATE_NEUTRAL,
  STATE_NEGATIVE
};

// ── LED Color Presets ──────────────────────────────────────────
struct RGBColor { uint8_t r, g, b; };

constexpr RGBColor LED_BOOT      = {255, 255, 255};  // white
constexpr RGBColor LED_IDLE      = {  0,  20,  40};  // dim blue
constexpr RGBColor LED_LISTENING = {  0,   0, 200};  // bright blue
constexpr RGBColor LED_POSITIVE  = {  0, 200,  50};  // green
constexpr RGBColor LED_NEUTRAL   = {180, 180,   0};  // yellow
constexpr RGBColor LED_NEGATIVE  = {200,   0,   0};  // red
