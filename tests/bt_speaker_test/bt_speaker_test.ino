/**
 * Emo Robot — Bluetooth Speaker Test
 * 
 * Tests if ESP32 can connect to "Mini boost 4" 
 * and produce audio via Bluetooth A2DP.
 * 
 * Library required: ESP32-A2DP by Phil Schatzmann
 * Install via: Tools → Manage Libraries → "ESP32-A2DP"
 */

#include "BluetoothA2DPSource.h"
#include <math.h>

BluetoothA2DPSource a2dp_source;

// ── Tone config ──────────────────────────────────────────
#define TONE_FREQUENCY   440.0f   // A4 note (Hz)
#define SAMPLE_RATE    44100.0f
#define VOLUME           8000     // 0 to 32767 (half volume)
// ─────────────────────────────────────────────────────────

static float phase = 0.0f;

// Called by A2DP library to get audio samples
int32_t get_sound_data(Frame *data, int32_t frame_count) {
    for (int i = 0; i < frame_count; i++) {
        int16_t sample = (int16_t)(sinf(phase) * VOLUME);
        data[i].channel1 = sample;  // Left
        data[i].channel2 = sample;  // Right

        phase += 2.0f * M_PI * TONE_FREQUENCY / SAMPLE_RATE;
        if (phase >= 2.0f * M_PI) {
            phase -= 2.0f * M_PI;
        }
    }
    return frame_count;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("=================================");
    Serial.println("   Emo Robot — BT Speaker Test   ");
    Serial.println("=================================");
    Serial.println("Searching for: Mini boost 4 ...");
    Serial.println("(Make sure speaker is in pairing mode)");

    // Connect to speaker and start sending audio
    a2dp_source.start("Mini boost 4", get_sound_data);

    Serial.println("A2DP started. Waiting for connection...");
}

void loop() {
    if (a2dp_source.is_connected()) {
        Serial.println("✓ Connected! You should hear a 440Hz tone.");
    } else {
        Serial.println("... Still connecting ...");
    }
    delay(2000);
}
