/**
 * Emo V2 — Audio Player
 *
 * Raw PCM (22050 Hz, 16-bit, mono) from backend
 * → 16 KB lock-free ring buffer
 * → Pre-buffer ~185ms before playback starts (absorbs jitter)
 * → A2DP callback: 2× sample duplication → 44100 Hz stereo
 * → Bluetooth speaker ("Mini boost 4")
 */
#pragma once

#include "BluetoothA2DPSource.h"
#include "../config.h"

// ── Ring buffer ───────────────────────────────────────────
// 16 KB = 8192 int16 samples = ~371 ms at 22050 Hz
#define RING_BYTES    (16 * 1024)
#define RING_SAMPLES  (RING_BYTES / 2)

// Pre-buffer: wait until this many samples are in ring before starting playback.
// ~185ms cushion — absorbs network jitter regardless of root cause.
#define PREBUFFER_SAMPLES  (RING_SAMPLES / 4)

static int16_t  _ring[RING_SAMPLES];
static volatile int      _ringHead = 0;   // written by main task (WebSocket)
static volatile int      _ringTail = 0;   // read by A2DP callback on Core 0
static volatile bool     _streaming  = false;  // true while chunks arriving
static volatile bool     _streamDone = false;  // true after audio_end received
static volatile bool     _drainDone  = false;  // signals loop() to print "done"
static volatile bool     _playbackStarted = false;  // false until pre-buffer threshold met
static volatile uint32_t _totalSamplesReceived = 0;
static bool              _lastConnected = false;

static BluetoothA2DPSource _a2dp;

// ── Helpers ──────────────────────────────────────────────
static inline int _ringAvail() {
    return (_ringHead - _ringTail + RING_SAMPLES) % RING_SAMPLES;
}
static inline int _ringSpace() {
    return RING_SAMPLES - 1 - _ringAvail();
}

// ── A2DP Callback — runs on Core 0 (BT task) ─────────────
// Each input sample → 2 A2DP stereo frames (22050 → 44100 Hz)
// Mono → stereo: channel1 = channel2 = sample
static int32_t _a2dpCallback(Frame* frames, int32_t count) {

    // Pre-buffer gate: output silence until enough data is buffered
    if (!_playbackStarted) {
        if (_ringAvail() >= PREBUFFER_SAMPLES || _streamDone) {
            _playbackStarted = true;  // Cushion ready — start playing!
        } else {
            for (int i = 0; i < count; i++) {
                frames[i].channel1 = 0;
                frames[i].channel2 = 0;
            }
            return count;
        }
    }

    int filled = 0;

    while (filled + 1 < count) {   // room for 2 stereo frames per sample
        if (_ringAvail() == 0) {
            if (_streamDone) {
                _drainDone  = true;
                _streamDone = false;
            }
            break;
        }
        int16_t s = _ring[_ringTail];
        _ringTail = (_ringTail + 1) % RING_SAMPLES;

        // 2× upsampling + mono→stereo
        frames[filled].channel1 = s;
        frames[filled].channel2 = s;
        filled++;
        frames[filled].channel1 = s;
        frames[filled].channel2 = s;
        filled++;
    }

    // Pad with silence when no samples ready
    for (int i = filled; i < count; i++) {
        frames[i].channel1 = 0;
        frames[i].channel2 = 0;
    }
    return count;
}

// ════════════════════════════════════════════════════════
class AudioPlayerClass {
public:

    // Call in setup() — BEFORE WiFi for stable coexistence
    void begin() {
        _a2dp.start(BT_SPEAKER_NAME, _a2dpCallback);
        Serial.println("[AUDIO] A2DP started. Searching for '" BT_SPEAKER_NAME "'...");
    }

    // ── backend sends {"type":"audio_start"} ─────────────
    void onAudioStart() {
        _ringHead   = 0;
        _ringTail   = 0;
        _streamDone = false;
        _drainDone  = false;
        _playbackStarted = false;   // Wait for pre-buffer threshold
        _streaming  = true;
        _totalSamplesReceived = 0;
        Serial.println("[AUDIO] ▶ Stream started (pre-buffering...)");
    }

    // ── backend sends binary chunk (raw PCM) ─────────────
    void onPCMChunk(const uint8_t* data, size_t len) {
        if (!_streaming) return;

        int numSamples = len / 2;
        const int16_t* samples = (const int16_t*)data;

        int space = _ringSpace();
        int toCopy = min(numSamples, space);

        for (int i = 0; i < toCopy; i++) {
            _ring[_ringHead] = samples[i];
            _ringHead = (_ringHead + 1) % RING_SAMPLES;
        }

        _totalSamplesReceived += toCopy;
    }

    // ── backend sends {"type":"audio_end"} ───────────────
    void onAudioEnd() {
        _streaming  = false;
        _streamDone = true;   // A2DP callback will set _drainDone when ring drains
        Serial.printf("[AUDIO] Stream end — %u samples total (~%d ms)\n",
                      _totalSamplesReceived,
                      (int)(_totalSamplesReceived * 1000UL / 22050UL));
    }

    void loop() {
        // Monitor BT connection state changes
        bool connected = _a2dp.is_connected();
        if (connected != _lastConnected) {
            _lastConnected = connected;
            if (connected) {
                Serial.printf("\n[AUDIO] ✓ Bluetooth Speaker '%s' CONNECTED!\n", BT_SPEAKER_NAME);
            } else {
                Serial.printf("\n[AUDIO] ✗ Bluetooth Speaker '%s' DISCONNECTED!\n", BT_SPEAKER_NAME);
            }
        }

        if (_drainDone) {
            _drainDone = false;
            Serial.println("[AUDIO] ■ Playback done");
        }
    }

    bool isPlaying()   { return _streaming || _streamDone || (_ringAvail() > 0); }
    bool isConnected() { return _a2dp.is_connected(); }
};

AudioPlayerClass audioPlayer;
