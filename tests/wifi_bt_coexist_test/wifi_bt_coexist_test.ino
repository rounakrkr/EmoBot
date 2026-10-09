/**
 * WiFi + Bluetooth A2DP Coexistence Test
 * 
 * Verifies if this ESP32 can connect to both:
 * 1. WiFi (WIFI_SSID from config.h)
 * 2. BT Speaker (Mini boost 4)
 * 
 * And prints free heap every 2 seconds.
 */

#include <WiFi.h>
#include "BluetoothA2DPSource.h"
#include <math.h>

#include "../../firmware/config.h"   // WIFI_SSID / WIFI_PASSWORD (copy config.example.h -> config.h)
#define BT_NAME       "Mini boost 4"

BluetoothA2DPSource a2dp;

static float phase = 0.0f;
static bool playTone = false;

// 440 Hz tone generator for A2DP
int32_t get_sound_data(Frame *data, int32_t frame_count) {
    if (!playTone) {
        for (int i = 0; i < frame_count; i++) {
            data[i].channel1 = 0;
            data[i].channel2 = 0;
        }
        return frame_count;
    }
    for (int i = 0; i < frame_count; i++) {
        int16_t sample = (int16_t)(sinf(phase) * 6000);
        data[i].channel1 = sample;
        data[i].channel2 = sample;
        phase += 2.0f * M_PI * 440.0f / 44100.0f;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    return frame_count;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n--- WiFi + BT Coexistence Test ---");
    Serial.printf("Initial Free Heap: %u bytes\n", ESP.getFreeHeap());

    // 1. WiFi First
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nWiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Free Heap after WiFi: %u bytes\n", ESP.getFreeHeap());

    // 2. Bluetooth A2DP
    Serial.printf("Starting A2DP to '%s'...\n", BT_NAME);
    a2dp.start(BT_NAME, get_sound_data);
    Serial.printf("Free Heap after A2DP start: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
        lastPrint = millis();
        bool btConnected = a2dp.is_connected();
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);
        
        Serial.printf("[STATUS] WiFi: %s | BT: %s | Free Heap: %u bytes | Min Heap: %u bytes\n",
            wifiConnected ? "CONNECTED" : "DISCONNECTED",
            btConnected ? "CONNECTED (playing tone)" : "connecting...",
            ESP.getFreeHeap(),
            ESP.getMinFreeHeap()
        );

        if (btConnected) {
            playTone = true; // play tone once connected
        }
    }
}
