/**
 * Emo V2 — Audio Player  (download-then-play)
 *
 * Why: WiFi RX and A2DP TX fight over the single ESP32 radio. Streaming live
 * caused 3-9 s WiFi stalls (=> audible stutter / WS disconnects).
 *
 * Flow per reply:
 *   audio_start  -> open /clip.pcm in LittleFS, SUSPEND the A2DP stream
 *                   (radio is free, WiFi gets its full ~80 KB/s)
 *   PCM chunks   -> buffered (4 KB) and written to flash
 *   audio_end    -> close file, resume A2DP (CHECK_SRC_RDY -> START)
 *   playback     -> reader task: flash -> small ring -> A2DP callback
 *                   (2x upsample + mono->stereo). No WiFi traffic needed,
 *                   and servo delay()s in loop() can't starve the audio.
 *
 * Needs: Partition Scheme "Huge APP (3MB No OTA / 1MB SPIFFS)" (LittleFS).
 * Input format: raw PCM 22050 Hz, 16-bit, mono.
 */
#pragma once

#include "BluetoothA2DPSource.h"
#include "esp_a2dp_api.h"
#include <LittleFS.h>
#include "../config.h"

// ── Tunables ─────────────────────────────────────────────
#define CLIP_PATH              "/clip.pcm"
#define MAX_CLIP_BYTES         (600UL * 1024UL)   // ~13.6 s, fits 1 MB LittleFS
#define RING_SAMPLES           4096               // 8 KB ring (~186 ms)
#define PREBUFFER_SAMPLES      1536               // ~70 ms before first sound
#define WBUF_BYTES             4096               // flash write buffer
#define DOWNLOAD_TIMEOUT_MS    15000              // no chunk for this long -> give up
#define AUDIO_SUSPEND_A2DP     0                  // 0 = keep A2DP streaming during download (avoids suspend packet drop)
#define AUDIO_DEBUG_TIMING     1                  // log chunk arrival / flash write timings
#define RESUME_STEP_TIMEOUT_MS 2500
#define RESUME_MAX_TRIES       3

enum ApState { AP_IDLE, AP_DOWNLOADING, AP_RESUMING, AP_PLAYING };

static volatile ApState  _apState = AP_IDLE;

// ── Ring (single producer: reader task, single consumer: A2DP callback)
static int16_t           _ring[RING_SAMPLES];
static volatile int      _ringHead = 0;
static volatile int      _ringTail = 0;
static volatile bool     _playActive      = false;  // callback may consume ring
static volatile bool     _playbackStarted = false;  // pre-buffer gate passed
static volatile bool     _fileEof         = false;  // reader hit end of file
static volatile bool     _readerActive    = false;
static volatile bool     _drainDone       = false;

static volatile esp_a2d_audio_state_t _btState = ESP_A2D_AUDIO_STATE_STOPPED;

// ── Download / resume bookkeeping (main task only)
static uint8_t   _wbuf[WBUF_BYTES];
static size_t    _wbufLen      = 0;
static File      _wfile, _rfile;
static uint32_t  _clipBytes    = 0;
static bool      _btSuspended  = false;
static uint32_t  _lastChunkMs  = 0;
static uint32_t  _stepMs       = 0;
static uint32_t  _playStartMs  = 0;
static uint8_t   _resumeStep   = 0;
static uint8_t   _resumeTries  = 0;
static bool      _lastConnected = false;
static uint32_t  _dlStartMs    = 0;
static uint16_t  _dlChunks     = 0;
static int       _btStatePrinted = -1;

static BluetoothA2DPSource _a2dp;

static inline int _ringAvail() { return (_ringHead - _ringTail + RING_SAMPLES) % RING_SAMPLES; }
static inline int _ringSpace() { return RING_SAMPLES - 1 - _ringAvail(); }

static void _onA2dpAudioState(esp_a2d_audio_state_t state, void*) { _btState = state; }

// ── A2DP data callback — BT task (Core 0) ────────────────
// Each input sample -> 2 stereo frames (22050 -> 44100 Hz, mono -> stereo)
static int32_t _a2dpCallback(Frame* frames, int32_t count) {
    if (!_playActive) {                       // nothing to play: silence
        for (int i = 0; i < count; i++) { frames[i].channel1 = 0; frames[i].channel2 = 0; }
        return count;
    }

    if (!_playbackStarted) {                  // pre-buffer gate
        if (_ringAvail() >= PREBUFFER_SAMPLES || _fileEof) {
            _playbackStarted = true;
        } else {
            for (int i = 0; i < count; i++) { frames[i].channel1 = 0; frames[i].channel2 = 0; }
            return count;
        }
    }

    int filled = 0;
    while (filled + 1 < count) {
        if (_ringAvail() == 0) {
            if (_fileEof) {                   // file fully read AND ring drained
                _playActive = false;
                _drainDone  = true;
            }
            break;
        }
        int16_t s = _ring[_ringTail];
        _ringTail = (_ringTail + 1) % RING_SAMPLES;
        frames[filled].channel1 = s; frames[filled].channel2 = s; filled++;
        frames[filled].channel1 = s; frames[filled].channel2 = s; filled++;
    }
    for (int i = filled; i < count; i++) { frames[i].channel1 = 0; frames[i].channel2 = 0; }
    return count;
}

// ── Reader task: flash -> ring ───────────────────────────
static void _readerTask(void*) {
    uint8_t tmp[512];
    for (;;) {
        if (_readerActive) {
            int space = _ringSpace();
            if (space >= 256) {
                int n = _rfile.read(tmp, 512);
                if (n > 1) {
                    const int16_t* s = (const int16_t*)tmp;
                    int h = _ringHead;
                    for (int i = 0; i < n / 2; i++) { _ring[h] = s[i]; h = (h + 1) % RING_SAMPLES; }
                    __sync_synchronize();
                    _ringHead = h;
                    continue;
                }
                _fileEof      = true;         // EOF (or read error)
                _readerActive = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

// ════════════════════════════════════════════════════════
class AudioPlayerClass {
public:
    // Call in setup() — BEFORE WiFi for stable coexistence
    void begin() {
        if (!LittleFS.begin(true)) {          // true = format on first use
            Serial.println("[AUDIO] ✗ LittleFS mount failed — check Partition Scheme!");
        } else {
            Serial.printf("[AUDIO] LittleFS ok: %u / %u bytes free\n",
                          (unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes()),
                          (unsigned)LittleFS.totalBytes());
            LittleFS.remove(CLIP_PATH);
        }
        xTaskCreatePinnedToCore(_readerTask, "audioRd", 4096, nullptr, 2, nullptr, 1);
        _a2dp.set_on_audio_state_changed(_onA2dpAudioState);
        _a2dp.start(BT_SPEAKER_NAME, _a2dpCallback);
        Serial.println("[AUDIO] A2DP started. Searching for '" BT_SPEAKER_NAME "'...");
    }

    // ── {"type":"audio_start"} ───────────────────────────
    void onAudioStart() {
        _stopAll();                                   // abort anything in flight
        LittleFS.remove(CLIP_PATH);
        _wfile = LittleFS.open(CLIP_PATH, FILE_WRITE);
        if (!_wfile) {
            Serial.println("[AUDIO] ✗ Cannot open clip file — dropping this reply's audio");
            return;                                   // stay IDLE; chunks ignored
        }
        _wbufLen = 0; _clipBytes = 0;
        _lastChunkMs = millis();
        _apState = AP_DOWNLOADING;

#if AUDIO_SUSPEND_A2DP
        if (_a2dp.is_connected() && !_btSuspended) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);   // free the radio
            _btSuspended = true;
        }
#endif
        _dlStartMs = millis(); _dlChunks = 0;
        Serial.printf("[AUDIO] ⬇ Downloading clip (A2DP %s, bt_state=%d, heap=%u)\n",
                      _btSuspended ? "suspended" : "NOT suspended", (int)_btState, (unsigned)ESP.getFreeHeap());
    }

    // ── binary chunk (raw PCM) ───────────────────────────
    void onPCMChunk(const uint8_t* data, size_t len) {
        if (_apState != AP_DOWNLOADING) return;
        _lastChunkMs = millis();
#if AUDIO_DEBUG_TIMING
        if (_dlChunks < 8) Serial.printf("[AUDIO]   chunk #%u @ +%u ms\n", _dlChunks + 1, (unsigned)(_lastChunkMs - _dlStartMs));
#endif
        _dlChunks++;
        while (len > 0) {
            if (_clipBytes + _wbufLen >= MAX_CLIP_BYTES) return;     // cap: drop the rest
            size_t n = min(len, (size_t)(WBUF_BYTES - _wbufLen));
            memcpy(_wbuf + _wbufLen, data, n);
            _wbufLen += n; data += n; len -= n;
            if (_wbufLen == WBUF_BYTES) _flushWrite();
        }
        _lastChunkMs = millis();      // a slow flash write must not count as "no data"
    }

    // ── {"type":"audio_end"} ─────────────────────────────
    void onAudioEnd() {
        if (_apState != AP_DOWNLOADING) return;
        _finishDownload();
    }

    // Call every loop()
    void loop() {
        uint32_t now = millis();

        bool connected = _a2dp.is_connected();
        if (connected != _lastConnected) {
            _lastConnected = connected;
            Serial.printf("\n[AUDIO] %s Bluetooth Speaker '%s' %s!\n",
                          connected ? "✓" : "✗", BT_SPEAKER_NAME,
                          connected ? "CONNECTED" : "DISCONNECTED");
            if (!connected) _btSuspended = false;     // stream state is gone with the link
        }

        if ((int)_btState != _btStatePrinted) {
            _btStatePrinted = (int)_btState;
            Serial.printf("[AUDIO] bt_state -> %d (0=remote_suspend 1=stopped 2=started)\n", _btStatePrinted);
        }

        switch (_apState) {
        case AP_DOWNLOADING:
            if (now - _lastChunkMs > DOWNLOAD_TIMEOUT_MS) {
                Serial.println("[AUDIO] ⚠ Download timeout — playing what we have");
                _finishDownload();
            }
            break;

        case AP_RESUMING:
            _stepResume(now);
            break;

        case AP_PLAYING:
            if (_drainDone) {
                _drainDone = false;
                _endPlayback();
                Serial.println("[AUDIO] ■ Playback done");
            } else if (now - _playStartMs > (_clipBytes / 44UL) + 4000UL || !connected) {
                Serial.println("[AUDIO] ⚠ Playback watchdog — aborting");
                _endPlayback();
            }
            break;

        default: break;
        }
    }

    // True for the whole reply: download + resume + playback
    bool isPlaying()   { return _apState != AP_IDLE; }
    bool isConnected() { return _a2dp.is_connected(); }

private:
    void _flushWrite() {
        if (_wbufLen == 0) return;
        uint32_t t0 = millis();
        size_t w = _wfile.write(_wbuf, _wbufLen);
#if AUDIO_DEBUG_TIMING
        uint32_t dt = millis() - t0;
        if (dt > 60) Serial.printf("[AUDIO]   ⚠ flash write took %u ms (total so far %u B)\n", (unsigned)dt, (unsigned)(_clipBytes + w));
#endif
        if (w != _wbufLen) Serial.printf("[AUDIO] ⚠ Flash write short: %u/%u\n", (unsigned)w, (unsigned)_wbufLen);
        _clipBytes += w;
        _wbufLen = 0;
    }

    void _finishDownload() {
        _flushWrite();
        _wfile.close();
        Serial.printf("[AUDIO] ⬇ Clip saved: %u bytes (~%u ms audio) in %u ms, %u chunks\n",
                      (unsigned)_clipBytes, (unsigned)(_clipBytes * 1000UL / 44100UL),
                      (unsigned)(millis() - _dlStartMs), (unsigned)_dlChunks);
        _resumeStep = 0; _resumeTries = 0;
        _apState = (_clipBytes >= 2) ? AP_RESUMING : AP_IDLE;
        if (_apState == AP_IDLE && _btSuspended) {    // nothing to play: resume stream anyway
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
            _btSuspended = false;
        }
    }

    void _stepResume(uint32_t now) {
        if (!_btSuspended) { _beginPlayback(); return; }
        if (_resumeStep == 0) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
            _stepMs = now; _resumeStep = 1;
        } else if (_resumeStep == 1) {
            if (now - _stepMs >= 80) {
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                _stepMs = now; _resumeStep = 2;
            }
        } else {
            if (_btState == ESP_A2D_AUDIO_STATE_STARTED) {
                _btSuspended = false;
                Serial.println("[AUDIO] ▶ A2DP resumed");
                _beginPlayback();
            } else if (now - _stepMs > RESUME_STEP_TIMEOUT_MS) {
                if (++_resumeTries < RESUME_MAX_TRIES) {
                    Serial.printf("[AUDIO] ⚠ Resume retry %u...\n", _resumeTries);
                    _resumeStep = 0;
                } else {
                    Serial.println("[AUDIO] ✗ A2DP did not resume — dropping clip");
                    _btSuspended = false;
                    _apState = AP_IDLE;
                }
            }
        }
    }

    void _beginPlayback() {
        if (!_a2dp.is_connected()) {
            Serial.println("[AUDIO] Speaker not connected — dropping clip");
            _apState = AP_IDLE;
            return;
        }
        _rfile = LittleFS.open(CLIP_PATH, FILE_READ);
        if (!_rfile) {
            Serial.println("[AUDIO] ✗ Cannot open clip for playback");
            _apState = AP_IDLE;
            return;
        }
        _ringHead = 0; _ringTail = 0;
        _playbackStarted = false; _fileEof = false; _drainDone = false;
        _playStartMs = millis();
        _apState = AP_PLAYING;
        _playActive   = true;                         // enable consumer first...
        _readerActive = true;                         // ...then producer
        Serial.println("[AUDIO] ▶ Playing from flash");
    }

    void _endPlayback() {
        _playActive = false; _readerActive = false;
        vTaskDelay(pdMS_TO_TICKS(10));                // let reader task leave _rfile
        if (_rfile) _rfile.close();
        _apState = AP_IDLE;
    }

    // Abort whatever is happening (new reply arrived, etc). A2DP suspend state is kept.
    void _stopAll() {
        _playActive = false; _readerActive = false;
        vTaskDelay(pdMS_TO_TICKS(10));
        if (_rfile) _rfile.close();
        if (_wfile) _wfile.close();
        _wbufLen = 0;
        _apState = AP_IDLE;
    }
};

AudioPlayerClass audioPlayer;
