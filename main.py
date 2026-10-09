import os
import sys
import time
import signal
import multiprocessing

# Force main process to CPU-only initially
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

import esp_controller

IDLE_TIMEOUT = 10
last_interaction = time.time()
interaction_queue = multiprocessing.Queue()
tts_speaking = multiprocessing.Event()
esp = None

def check_idle():
    global last_interaction, esp
    try:
        if interaction_queue.get_nowait() == "USER_SPOKE":
            print("👤 User interaction detected")
            last_interaction = time.time()
    except:
        pass
    
    if time.time() - last_interaction > IDLE_TIMEOUT:
        print("⏳ No interaction for 10s → sending idle")
        if esp:
            esp.idle()
        last_interaction = time.time() + 1000000

def run(show_emotion_display=False):
    global esp 

    import emotion_module
    import speech_module
    import llm_module
    import tts_module

    esp = esp_controller.ESP32Controller("10.244.229.107")

    llm_queue = multiprocessing.Queue(maxsize=10)
    tts_queue = multiprocessing.Queue(maxsize=10)

    manager = multiprocessing.Manager()
    emotion_dict = manager.dict({'current': 'neutral'})
    emotion_lock = manager.Lock()

    emotion_stop = multiprocessing.Event()
    speech_stop = multiprocessing.Event()
    tts_stop = multiprocessing.Event()
    speech_ready = multiprocessing.Event()

    emotion_process = multiprocessing.Process(
        target=emotion_module.run_emotion,
        args=(emotion_stop, emotion_dict, emotion_lock, 0.7, 0.6, show_emotion_display)
    )
    
    speech_process = multiprocessing.Process(
        target=speech_module.start,
        args=(llm_queue, emotion_dict, emotion_lock, speech_stop, "cuda", "float16", speech_ready, tts_queue, tts_speaking, esp, interaction_queue)
    )
    
    tts_process = multiprocessing.Process(
        target=tts_module.start,
        args=(tts_queue, tts_stop, tts_speaking)
    )

    emotion_process.start()
    speech_process.start()

    if not speech_ready.wait(timeout=60):
        print("⚠️ Speech module not ready within 60s, continuing anyway...")

    llm_module.start(llm_queue, tts_queue, esp)
    tts_process.start()

    print("\n🚀 All modules started. Press Ctrl+C to stop.\n")

    def signal_handler(sig, frame):
        print("\n🛑 Shutting down...")
        emotion_stop.set()
        speech_stop.set()
        tts_stop.set()
        llm_module.stop()
        
        for p in [emotion_process, speech_process, tts_process]:
            p.join(timeout=3)
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    try:
        while True:
            time.sleep(1)
            check_idle()
    except KeyboardInterrupt:
        signal_handler(None, None)

if __name__ == "__main__":
    multiprocessing.freeze_support()
    run(show_emotion_display=True)