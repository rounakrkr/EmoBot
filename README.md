# 🤖 Aino - Modular AI Companion with Emotion Awareness

**Aino** is a personalized AI companion that combines computer vision, hybrid speech processing, and hardware-level interaction. Designed and developed by **Rounak Kumar**, a Computer Science student at **KIIT University**, this project focuses on creating a responsive and "alive" robot assistant.

---

## 🌟 Key Features

* **🎭 Emotion Detection:** Real-time facial expression analysis using DeepFace to adapt AI responses.
* **🧠 Cloud & Local Memory:** Hybrid memory system using JSONBin for cloud storage and local sliding-window memory for context.
* **🎙️ Hybrid Speech (STT/TTS):** Fast speech-to-text via Groq Whisper with local fallback, and high-quality voice output using ElevenLabs or YourTTS.
* **⚡ Optimized Performance:** Multiprocessing architecture to handle GPU-heavy tasks (Vision/STT) and CPU-only tasks (LLM/Control) simultaneously.
* **🤖 ESP32 Hardware Control:** Dedicated firmware for servo-controlled movements (tilt/rotate), OLED eye expressions, and RGB state indicators.

---

## 🛠️ Tech Stack

* **Languages:** Python (Core Logic), C++ (ESP32 Firmware)
* **AI/ML:** Faster-Whisper, DeepFace, Groq API, ElevenLabs
* **Hardware:** ESP32, SG90 Servos, SSD1306 OLED Displays, RGB LEDs
* **Backend:** Flask, OpenCV, requests

---

## 📂 Project Structure

### Python Modules (Brain)
- `main.py`: The orchestrator managing all processes and idle timers.
- `llm_module.py`: Handles conversation logic via Groq (Online) or Ollama (Offline).
- `speech_module.py`: Manages the wake word ("Alexa") and voice command detection.
- `emotion_module.py`: Processes camera frames for "Positive", "Neutral", or "Negative" states.
- `tts_module.py`: Handles voice generation and audio playback.
- `cloud_memory.py` / `memory.py`: Long-term and short-term memory management.

### ESP32 Firmware (Body)
Located in `HardwareControlFiles/`:
- `config.h`: Pin definitions and WiFi configurations.
- `eyes.h`: OLED drawing logic for various expressions (Happy, Sad, Blink).
- `servos.h`: Smooth movement logic for looking around and nodding.
- `led.h`: Pulse and flash effects for the RGB status indicator.

---

## 🚀 Setup Instructions

1.  **Clone the Repo:**
    ```bash
    git clone [https://github.com/rounakrkr/Aino-Brain-Lab.git](https://github.com/rounakrkr/Aino-Brain-Lab.git)
    ```

2.  **Environment Variables:**
    Create a `.env` file and add your API keys (see `.env.dummy` for reference).

3.  **Install Python Dependencies:**
    ```bash
    pip install -r requirements.txt
    ```

4.  **Flash ESP32:**
    Open the `.ino` file in Arduino IDE and upload it to your ESP32.

5.  **Run:**
    ```bash
    python main.py
    ```

---
