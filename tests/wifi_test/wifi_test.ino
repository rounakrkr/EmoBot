/**
 * Emo Robot — Personal Hotspot WiFi Test
 *
 * Simple WPA2-Personal connection test.
 * Credentials in: firmware/config.h
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "../../firmware/config.h"

void setup() {
    Serial.begin(DEBUG_BAUD_RATE);
    delay(500);

    Serial.println("\n=================================");
    Serial.println("  Emo Robot — WiFi Test (Hotspot)");
    Serial.println("=================================");
    Serial.print("Connecting to: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n✓ Connected!");
    Serial.print("  IP Address   : "); Serial.println(WiFi.localIP());
    Serial.print("  Signal (RSSI): "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");

    // Internet check
    Serial.println("\nChecking internet...");
    HTTPClient http;
    http.begin("http://httpbin.org/get");
    http.setTimeout(5000);
    int code = http.GET();

    if (code == 200) {
        Serial.println("✓ Internet confirmed! ESP32 can reach Oracle Cloud.");
    } else {
        Serial.print("⚠ HTTP code: "); Serial.println(code);
    }
    http.end();
}

void loop() {
    delay(5000);
    Serial.print("Connected | RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}
