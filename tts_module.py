import os
import io
import time
import queue
import numpy as np
import sounddevice as sd
import librosa
import multiprocessing
import torch
from TTS.api import TTS
from elevenlabs import ElevenLabs
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

# --- Configuration ---
MODE = "online"  # "online", "offline", "hybrid"
ELEVENLABS_API_KEY = os.getenv("ELEVENLABS_API_KEY")

ELEVENLABS_VOICES = {
    "neutral": "8G3CCzxBA6qtsfZGEvtF",
    "happy": "JBFqnCBsd6RMkjVDRZzb",
    "sad": "JBFqnCBsd6RMkjVDRZzb",
    "angry": "JBFqnCBsd6RMkjVDRZzb",
    "surprised": "JBFqnCBsd6RMkjVDRZzb",
}
ELEVENLABS_MODEL = "eleven_multilingual_v2"
ELEVENLABS_OUTPUT_FORMAT = "mp3_44100_128"

os.environ["COQUI_TOS_AGREEMENT"] = "y"
torch.backends.cudnn.benchmark = True
_eleven_client = None

def init_elevenlabs():
    global _eleven_client
    if not ELEVENLABS_API_KEY:
        print("⚠️ ELEVENLABS_API_KEY not set, online mode disabled.")
        return False
    try:
        _eleven_client = ElevenLabs(api_key=ELEVENLABS_API_KEY)
        print("✅ ElevenLabs client initialized.")
        return True
    except Exception as e:
        print(f"❌ Failed to initialize ElevenLabs: {e}")
        return False

def play_elevenlabs_audio(audio_bytes, tts_event=None):
    try:
        audio_np, sr = librosa.load(io.BytesIO(audio_bytes), sr=None, mono=True)
        if tts_event: tts_event.set()
        try:
            sd.play(np.array(audio_np, dtype=np.float32), samplerate=sr)
            sd.wait()
        finally:
            if tts_event:
                tts_event.clear()
                time.sleep(0.5)
    except Exception as e:
        print(f"❌ Error playing ElevenLabs audio: {e}")

def generate_elevenlabs(text, emotion):
    if not _eleven_client: raise RuntimeError("ElevenLabs client not initialized.")
    voice_id = ELEVENLABS_VOICES.get(emotion, ELEVENLABS_VOICES['neutral'])
    audio = _eleven_client.text_to_speech.convert(
        voice_id=voice_id, text=text, model_id=ELEVENLABS_MODEL, output_format=ELEVENLABS_OUTPUT_FORMAT
    )
    return b''.join(audio)

def start(input_queue, stop_event, tts_event=None):
    os.environ.pop("CUDA_VISIBLE_DEVICES", None)
    use_online = MODE in ("online", "hybrid")
    use_offline = MODE in ("offline", "hybrid")

    if use_online and not init_elevenlabs():
        if MODE == "online": return
        use_online = False

    tts, offline_sr = None, None
    if use_offline:
        print("⏳ Loading YourTTS (offline model)...")
        tts = TTS("tts_models/multilingual/multi-dataset/your_tts", gpu=torch.cuda.is_available())
        if torch.cuda.is_available(): tts.to('cuda')
        
        print("🔄 Warming up YourTTS...")
        torch.set_grad_enabled(False)
        with torch.inference_mode():
            tts.tts(text="Hello", speaker_wav="neutral.wav", language="en")
        offline_sr = getattr(tts.synthesizer, 'output_sample_rate', 16000)
        print("🔊 YourTTS ready.")

    while not stop_event.is_set():
        try: item = input_queue.get(timeout=0.5)
        except queue.Empty: continue

        if item is None: continue
        text, emotion = item
        print(f"🗣️ TTS speaking: {text} (emotion: {emotion})")

        if use_online:
            try:
                audio_bytes = generate_elevenlabs(text, emotion)
                play_elevenlabs_audio(audio_bytes, tts_event)
                continue
            except Exception as e:
                print(f"⚠️ ElevenLabs failed: {e}")
                if MODE == "online": continue

        if use_offline and tts:
            try:
                ref_file = f"{emotion}.wav" if os.path.exists(f"{emotion}.wav") else "neutral.wav"
                with torch.inference_mode():
                    audio = tts.tts(text=text, speaker_wav=ref_file, language="en")
                
                if tts_event: tts_event.set()
                try:
                    sd.play(np.array(audio, dtype=np.float32), samplerate=offline_sr)
                    sd.wait()
                finally:
                    if tts_event:
                        tts_event.clear()
                        time.sleep(0.3)
            except Exception as e:
                print(f"❌ YourTTS error: {e}")

def stop():
    print("🛑 TTS module stopped")