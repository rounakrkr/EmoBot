# 🤖 EmoBot — Interactive AI Desktop Companion Robot

**EmoBot** is a responsive, emotion-aware desktop companion robot powered by an **ESP32** microcontroller and an **Oracle Cloud** AI backend. It features dynamic OLED eye animations, servo-driven expressive head movements, real-time voice synthesis streaming over WebSockets, and Bluetooth speaker audio output.

Designed and developed by **Rounak Kumar**, Computer Science student at **KIIT University**.

---

## 🌟 Key Features

* **⚡ Cloud-Powered Intelligence:** Fast conversational AI pipeline hosted on Oracle Cloud using Groq LLMs (`openai/gpt-oss-20b`) and Groq Whisper STT.
* **🎙️ Real-Time Paced Audio Streaming:** High-quality voice synthesis via Microsoft Edge TTS (`en-US-AnaNeural`), streamed as raw PCM chunks over WebSocket to the ESP32.
* **🔊 Bluetooth A2DP Audio:** ESP32 streams audio directly to an external Bluetooth speaker with lock-free ring buffering, jitter pre-buffering, and hardware coexistence optimization.
* **👀 Dynamic OLED Expressions:** Dual SSD1306 displays rendering animated emotions (Happy, Sad, Curious, Angry, Neutral, Thinking).
* **🦾 Expressive Pan/Tilt Head:** SG90 servos providing natural head tilts, nods, and deferred emotional reactions synchronized with voice playback.
* **💡 Status RGB LED:** Multi-state color indication matching bot emotional state and system status.

---

## 🏗️ System Architecture

```text
┌────────────────┐          WebSocket          ┌──────────────────────────────────┐
│  NodeMCU ESP32 │ ◄─────────────────────────► │ Oracle Cloud Backend (FastAPI)   │
└───────┬────────┘    (Raw PCM 22050Hz + JSON) └────────────────┬─────────────────┘
        │                                                       │
        ├─► Dual SSD1306 OLED Displays (Eyes)                   ├─► Groq Whisper STT
        ├─► Dual SG90 Servos (Tilt/Rotate Head)                 ├─► Groq LLM (gpt-oss-20b)
        ├─► RGB LED (State Indicator)                           ├─► Edge TTS (AnaNeural)
        └─► Bluetooth Speaker (A2DP Audio Sink)                 └─► Session / Context Manager
```

---

## 📂 Project Structure

```text
EmoBot/
├── backend/                  # Cloud AI Backend (FastAPI)
│   ├── core/
│   │   ├── llm.py            # Groq LLM logic with JSON emotion parser
│   │   ├── pipeline.py       # STT → LLM → TTS paced audio streaming
│   │   ├── session.py        # Conversation history & context manager
│   │   ├── stt.py            # Speech-to-text integration
│   │   └── tts.py            # Edge TTS audio synthesizer
│   ├── routers/
│   │   └── ws_router.py      # Real-time WebSocket router with concurrency lock
│   ├── config.py             # Backend settings
│   ├── main.py               # FastAPI entrypoint
│   └── requirements.txt      # Backend Python dependencies
│
├── firmware/                 # ESP32 Firmware (PlatformIO / Arduino IDE)
│   ├── config.example.h      # Config template (copy to config.h — git-ignored)
│   └── emo_v2/
│       ├── emo_v2.ino        # Main sketch & state machine
│       ├── audio_player.h    # A2DP player with prebuffering & ring buffer
│       ├── eyes_v2.h         # OLED expressions & animations
│       ├── servos_v2.h       # Servo gestures (nods, head tilts)
│       └── led_v2.h          # RGB LED indicator routines
│
├── tests/                    # Diagnostic & hardware test sketches
│   ├── bt_speaker_test/      # Standalone Bluetooth speaker audio test
│   ├── wifi_bt_coexist_test/ # Simultaneous WiFi + A2DP coexistence test
│   └── wifi_test/            # WiFi connection diagnostic
│
├── CURRENT_ISSUES.md         # Active debugging log & hardware test status
├── README.md                 # Project documentation
├── requirements.txt          # Python dependencies wrapper
└── LICENSE                   # MIT License
```

---

## 🚀 Getting Started

### 1. Cloud Backend Setup
```bash
# Clone the repository
git clone https://github.com/rounakrkr/EmoBot.git
cd EmoBot/backend

# Create virtual environment & install dependencies
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Configure environment variables (.env)
cp ../.env.template .env
# Edit .env with your GROQ_API_KEY and preferences

# Start the backend server
uvicorn main:app --host 0.0.0.0 --port 8001
```

### 2. ESP32 Firmware Upload
1. Open `firmware/emo_v2/emo_v2.ino` in **Arduino IDE**.
2. Copy `firmware/config.example.h` to `firmware/config.h` (git-ignored) and fill in your WiFi credentials and backend IP.
3. Select board: **ESP32 Dev Module**.
4. Set Partition Scheme: **Huge APP (3MB No OTA / 1MB SPIFFS)**.
5. Click **Upload**.

---

## 📄 License
This project is licensed under the [MIT License](LICENSE).
