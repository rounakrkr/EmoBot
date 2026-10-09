# 📋 Current Issues & Debugging Log: Bluetooth Audio Jitter & Disconnect

> **Status:** 🟡 **Fixes Implemented & Deployed — Pending Hardware Verification**  
> **Last Updated:** October 2026

---

## 1. Issue Summary

Jab Emo robot backend se synthesized voice (Edge TTS `en-US-AnaNeural` -> raw PCM 22050Hz 16-bit mono) ko ESP32 ke Bluetooth speaker ("Mini boost 4" via A2DP) par play karta hai, tab do main problems aa rahi thi:

1. **Audio Jitter / Stutter ("Cut-cut ke aawaz aana"):**
   - Voice smooth nahi baj rahi thi, beech-beech me gaps aur stutter ho rahe the.
2. **First Exchange Disconnect:**
   - Pehli baar wake word / text input ke baad jab audio stream shuru hota tha, tab WebSocket drop ho jata tha:
     ```text
     [WS] ← audio_start
     [WS] Audio stream → 213798 bytes incoming
     [AUDIO] ▶ Stream started
     [WS] Disconnected — will retry...
     ```
   - Second exchange me generally chal jata tha.

---

## 2. Hardware & Environment Context

- **Microcontroller:** NodeMCU ESP32 (ESP32-WROOM-32D, **NO PSRAM**).
- **Usable Heap:** WiFi + Bluetooth Classic (A2DP) dono initialize hone ke baad sirf **~35KB DRAM** free rehta hai.
- **Network:** Mobile Hotspot (`RounakKR`).
- **Speaker:** Bluetooth Speaker "Mini boost 4" (A2DP sink).
- **Backend Server:** Oracle Cloud Ubuntu VM (`161.118.176.130:8001`), running FastAPI WebSocket.
- **Audio Format:** 22050 Hz, 16-bit, mono signed PCM. (ESP32 callback 2x upsample karke 44100 Hz stereo banata hai).

---

## 3. Root Causes & Technical Debate

Is issue ko troubleshoot karte waqt multiple layers par analysis aur debates hue:

### A. WiFi + Bluetooth Radio Coexistence
- **Problem:** ESP32 par single radio antenna share hota hai WiFi aur Bluetooth ke beech. Default WiFi modem sleep mode BT timing ko disturb karta hai.
- **Fix Applied:**
  - `WiFi.setSleep(false)` call kiya gaya.
  - `#include <esp_coexist.h>` ke sath `esp_coex_preference_set(ESP_COEX_PREFER_BT)` set kiya taaki hardware level par BT audio ko priority mile.

### B. Blocking Delays & Servo Gestures
- **Problem:** Audio start hone par `enterState()` me servo animation (`servoEnthusiasticNod()`) 1.5 seconds tak blocking `delay()` chala raha tha. Is duration me `ws.loop()` block ho jata tha, network frames drop hote the aur WebSocket disconnect trigger hota tha.
- **Fix Applied:**
  - Deferred gesture system banaya gaya (`_gesturePending`, `_audioStartedForGesture`).
  - Emotion ke gestures ab audio complete hone ke baad run hote hain.
  - Playback ke dauran eye blink delays ko bhi suppress kiya gaya jab tak `isPlaying()` true hai.

### C. Ring Buffer Memory Limits (WROOM DRAM Constraints)
- **Problem:** Buffer ko 32KB karne ki koshish par board crash (`StoreProhibited` Core 0 panic) ho gaya tha kyunki WROOM me PSRAM nahi hai aur internal DRAM exhaust ho jati hai.
- **Fix Applied:**
  - Buffer ko safe 16KB static allocation par rakha gaya (`RING_BYTES = 16 * 1024`, 8192 samples = ~371ms cushion).

### D. Buffer Overflow vs. Buffer Underrun Debate
- **Theoretical Calculation:** `sleep(0.012)` with 1024-byte chunks theoretical 85.3 KB/s bhejta, jabki playback rate 44.1 KB/s hai. Is calculation se laga ki buffer overflow ho raha hai.
- **Real-world Catch:** Server logs me dekha gaya ki ~118KB stream karne me actual wall-clock time ~5 seconds lag raha tha. Effective transfer rate sirf **~23.7 KB/s** tha (jo ki 44.1 KB/s se kam hai!).
- **Conclusion:** Yeh buffer overflow nahi, balki mobile hotspot latency ke chalte **buffer underrun** tha. Isliye blind pacing badhana (jaise `0.020s`) situation ko aur kharab kar sakta tha.
- **Solution:** `sleep(0.012)` ko touch nahi kiya gaya. Pehle precise `[PACING]` instrumentation lagayi gayi.

### E. WebSocket Frame Interleaving (Race Condition)
- **Problem:** Backend me background turn task (`pipeline.py`) se binary audio frames ja rahe the, jabki receive loop (`ws_router.py`) se `pong` frames concurrently ja rahe the without any mutex.
- **Consequence:** Corrupted WebSocket frames client (ESP32) ko milte the, jisse WebSocketsClient connection abruptly drop kar deta tha.
- **Fix Applied:**
  - Backend me single shared `ws_lock = asyncio.Lock()` introduce kiya gaya. Sabhi `send_json` aur `send_bytes` calls serialized ho gaye.

### F. Lenient Heartbeat & Pre-Buffering
- **Problem:** BT cold-start burst ke waqt 3 second ka heartbeat timeout bohot aggressive tha.
- **Fix Applied:**
  - `ws.enableHeartbeat(15000, 8000, 3)` (8 seconds pong timeout, 3 missed pings tolerance).
  - ESP32 side par pre-buffer logic add kiya gaya: `RING_SAMPLES / 4` (~185ms) samples jama hone ke baad hi playback start hota hai.

---

## 4. Current Codebase Changes Summary

| Component | File | Changes Implemented | Status |
|---|---|---|---|
| **Firmware** | `firmware/emo_v2/emo_v2.ino` | WiFi no sleep, coexistence priority BT, deferred gestures, lenient heartbeat (8s/3) | ✅ Code Ready |
| **Firmware** | `firmware/emo_v2/audio_player.h` | 16KB safe buffer, ~185ms pre-buffering gate before playback | ✅ Code Ready |
| **Backend** | `backend/core/pipeline.py` | Shared `ws_lock`, millisecond `[PACING]` logs every 20 chunks + summary | ✅ Deployed on Oracle |
| **Backend** | `backend/routers/ws_router.py` | Shared `ws_lock` passed to background pipeline tasks | ✅ Deployed on Oracle |

---

## 5. Next Testing Steps (User Verification)

Abhi user ne isko hardware par test nahi kiya hai. Testing ke waqt following points observe karne hain:

1. [ ] **Upload Firmware:** Arduino IDE me `emo_v2.ino` open karke `Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)` select karke ESP32 par flash karna.
2. [ ] **Check First Exchange Disconnect:** Serial Monitor me `hello emo` type karke dekhna ki `[WS] Disconnected — will retry` abhi bhi aata hai ya eliminate ho gaya.
3. [ ] **Check Voice Smoothness:** Bluetooth speaker par aawaz clean aa rahi hai ya abhi bhi "cut-cut" ke gaps hain.
4. [ ] **Backend PACING Logs:** Oracle server par logs check karna (`sudo journalctl -u emo-backend -f`) to verify:
   - Actual transfer rate (effective KB/s).
   - Delivery ratio vs real-time duration.
