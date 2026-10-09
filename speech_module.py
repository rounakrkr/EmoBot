import os
import sys
import time
import queue
import threading
import collections
import re
from collections import deque, Counter
import numpy as np
import sounddevice as sd
import webrtcvad
from playsound import playsound
from stt_groq import groq_stt

# --- Configuration ---
WAKE_WORD = "alexa"
SAMPLE_RATE = 16000
CHANNELS = 1
DTYPE = 'int16'
BLOCK_DURATION = 0.03
COMMAND_SILENCE_TIMEOUT = 1.15
PRE_SPEECH_BUFFER = 1.0
MIN_COMMAND_LENGTH = 0.8
VAD_AGGRESSIVENESS = 2
WAKE_CHECK_INTERVAL = 0.35
INITIAL_SPEECH_TIMEOUT = 5.0

# --- Global State ---
running = False
llm_queue, _stop_event, _emotion_dict, _emotion_lock = None, None, None, None
_esp, _interaction_queue, _tts_queue, _tts_event = None, None, None, None
_model = None

conversation_mode = False
audio_queue = queue.Queue(maxsize=500)
vad = webrtcvad.Vad(VAD_AGGRESSIVENESS)

command_active = False
command_buffer = []
silence_frames = 0
last_speech_time = 0
speech_started = False
command_emotion = None
last_wake_check = 0

pre_buffer = collections.deque(maxlen=int(PRE_SPEECH_BUFFER * SAMPLE_RATE))
emotion_history = deque(maxlen=100)

def audio_callback(indata, frames, time_info, status):
    if _tts_event and _tts_event.is_set():
        return
    if status.input_overflow:
        while not audio_queue.empty():
            try: audio_queue.get_nowait()
            except queue.Empty: break
        return
    try:
        audio_queue.put(indata.copy(), timeout=0.1)
    except queue.Full:
        while not audio_queue.empty():
            try: audio_queue.get_nowait()
            except queue.Empty: break

def check_for_wake_word(audio_chunk):
    if _model is None: return False
    samples_needed = int(1.0 * SAMPLE_RATE)
    recent_audio = list(pre_buffer)[-samples_needed:] if len(pre_buffer) >= samples_needed else list(pre_buffer)
    if len(recent_audio) < samples_needed * 0.5: return False

    audio_float = np.array(recent_audio, dtype=np.float32) / 32768.0
    try:
        segments, _ = _model.transcribe(audio_float, beam_size=1, language="en", temperature=0.0, condition_on_previous_text=False)
        text = " ".join(seg.text for seg in segments).strip().lower()
        text = re.sub(r'[.,?!]', '', text).strip()
        if any(word in {"alexa", "alex", "alexaa"} for word in text.split()):
            print(f"🎤 Wake word detected: '{text}'")
            return True
    except Exception as e:
        pass
    return False

def process_command():
    global command_buffer, command_active, command_emotion, speech_started, conversation_mode
    if not command_buffer or _model is None: return

    try:
        full_audio = np.concatenate(command_buffer)
        if (len(full_audio) / SAMPLE_RATE) < MIN_COMMAND_LENGTH: return

        print("📤 Sending to Groq Whisper API...")
        command_text = groq_stt.transcribe(full_audio.astype(np.float32) / 32768.0, SAMPLE_RATE)
        
        if not command_text:
            print("⚠️ Groq failed, using local STT...")
            segments, _ = _model.transcribe(full_audio.astype(np.float32) / 32768.0, beam_size=5, language="en", temperature=0.0, vad_filter=True)
            command_text = " ".join(seg.text for seg in segments).strip()
        
        if command_text:
            print(f"✅ Command: {command_text}")
            text_lower = command_text.lower()
            clean_command = re.sub(r'^alexa[\s,.!?]*', '', command_text, flags=re.IGNORECASE).strip()
            
            if len(command_text) < 3 or any(re.search(r'\b' + re.escape(p) + r'\b', text_lower) for p in ["thank you", "thanks", "okay", "ok", "sure"]):
                return
            
            if "focus mode" in text_lower:
                conversation_mode = True
                print("🎯 Focus mode activated")
                if _tts_queue: _tts_queue.put_nowait(("Focus mode activated.", "neutral"))
                return
            elif "exit focus" in text_lower and conversation_mode:
                conversation_mode = False
                print("🛑 Focus mode deactivated")
                if _tts_queue: _tts_queue.put_nowait(("Focus mode deactivated.", "neutral"))
                return
            
            if llm_queue:
                llm_queue.put_nowait((clean_command, command_emotion or "neutral"))
                if _interaction_queue: _interaction_queue.put_nowait("USER_SPOKE")
    except Exception as e:
        print(f"Error processing command: {e}")
    finally:
        command_buffer, command_emotion, speech_started = [], None, False

def listen_loop():
    global command_active, silence_frames, last_speech_time, speech_started, command_emotion, last_wake_check
    print("\n" + "="*50 + f"\n🎤 Voice Assistant Ready! Wake word: '{WAKE_WORD}'\n" + "="*50 + "\n")
    
    try:
        with sd.InputStream(samplerate=SAMPLE_RATE, channels=CHANNELS, dtype=DTYPE, callback=audio_callback, blocksize=int(SAMPLE_RATE * BLOCK_DURATION)):
            while not _stop_event.is_set():
                try: audio_block = audio_queue.get(timeout=0.2).flatten().astype(np.int16)
                except queue.Empty: continue
                pre_buffer.extend(audio_block)
                energy = np.abs(audio_block).mean()
                
                try:
                    with _emotion_lock: emotion_history.append((time.time(), _emotion_dict.get('current', 'neutral')))
                except Exception: pass

                try: is_speech = vad.is_speech(audio_block.tobytes(), SAMPLE_RATE)
                except Exception: continue

                if command_active:
                    command_buffer.append(audio_block)
                    if is_speech:
                        speech_started, silence_frames, last_speech_time = True, 0, time.time()
                    elif speech_started:
                        silence_frames += 1
                        if (silence_frames * BLOCK_DURATION) >= COMMAND_SILENCE_TIMEOUT:
                            process_command()
                            command_active = False
                    elif time.time() - last_speech_time > INITIAL_SPEECH_TIMEOUT:
                        command_active, speech_started, command_buffer = False, False, []
                    continue

                if conversation_mode and not command_active and is_speech and energy >= 0.013:
                    command_active, command_buffer, speech_started, silence_frames, last_speech_time = True, [], False, 0, time.time()
                    recent_emotions = [e for ts, e in emotion_history if ts >= time.time() - 3.0]
                    command_emotion = Counter(recent_emotions).most_common(1)[0][0] if recent_emotions else "neutral"
                    continue
                
                if energy >= 0.0105 and is_speech and not command_active:
                    now = time.time()
                    if now - last_wake_check > WAKE_CHECK_INTERVAL:
                        last_wake_check = now
                        if check_for_wake_word(audio_block):
                            recent_emotions = [e for ts, e in emotion_history if ts >= now - 3.0]
                            command_emotion = Counter(recent_emotions).most_common(1)[0][0] if recent_emotions else "neutral"
                            threading.Thread(target=playsound, args=("ding.mp3",), daemon=True).start()
                            if _esp: _esp.listening()
                            command_active, command_buffer, speech_started, silence_frames, last_speech_time = True, [], False, 0, time.time()
                            pre_list = list(pre_buffer)[-int(0.3 * SAMPLE_RATE):]
                            if pre_list: command_buffer.append(np.array(pre_list, dtype=np.int16))
                time.sleep(0.001)
    except Exception as e: print(f"❌ Audio stream error: {e}")

def start(queue, emotion_dict, lock, stop_event, device="cuda", compute_type="float16", ready_event=None, tts_queue=None, tts_event=None, esp=None, interaction_queue=None):
    global llm_queue, _emotion_dict, _emotion_lock, _stop_event, _model, _tts_queue, _tts_event, _esp, _interaction_queue
    _tts_event, _esp, _interaction_queue, llm_queue = tts_event, esp, interaction_queue, queue
    _emotion_dict, _emotion_lock, _stop_event = emotion_dict, lock, stop_event

    print(f"🔹 Initializing Speech Module (Device: {device})")
    from faster_whisper import WhisperModel
    if device == "cpu":
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
        compute_type = "float32" if compute_type == "float16" else compute_type

    try:
        _model = WhisperModel("base", device=device, compute_type=compute_type)
        _model.transcribe(np.zeros(SAMPLE_RATE, dtype=np.float32), beam_size=1)
        print("✅ WhisperModel loaded and warmed up!")
        if ready_event: ready_event.set()
    except Exception as e:
        print(f"❌ Whisper Model Error: {e}")
        return

    listen_loop()