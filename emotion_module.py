import os
import cv2
import time
import numpy as np
import threading
import multiprocessing
from collections import deque
from deepface import DeepFace

os.environ["TF_FORCE_GPU_ALLOW_GROWTH"] = "true"

def softmax(x):
    e_x = np.exp(x - np.max(x))
    return e_x / e_x.sum()

def run_emotion(stop_event, emotion_dict, lock, smoothing_factor=0.7, min_confidence=0.6, display=False):
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("❌ Emotion module: Could not open camera")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    print("\n" + "="*60)
    print("😊 EMOTION MODULE STARTED")
    print(f"Smoothing: {smoothing_factor} | Confidence: {min_confidence} | Detector: opencv")
    print("="*60 + "\n")

    categories = ['positive', 'neutral', 'negative']
    category_probs = np.array([0.33, 0.34, 0.33])
    current_emotion = 'neutral'
    previous_emotion = 'neutral'

    stable_frames_required = 5
    emotion_counts = {cat: 0 for cat in categories}
    prob_window = deque(maxlen=5)

    no_face_counter = 0
    last_change_time = time.time()
    last_print_time = time.time()

    while not stop_event.is_set():
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.1)
            continue

        display_frame = frame.copy() if display else None

        try:
            result = DeepFace.analyze(
                img_path=frame,
                actions=['emotion'],
                enforce_detection=False,
                detector_backend='opencv',
                silent=True
            )

            emotions = result[0]['emotion'] if isinstance(result, list) else result['emotion']

            if time.time() - last_print_time > 2.0:
                print(f"📊 Raw: happy={emotions['happy']:.1f} neu={emotions['neutral']:.1f} surp={emotions['surprise']:.1f} angry={emotions['angry']:.1f} sad={emotions['sad']:.1f} fear={emotions['fear']:.1f} disgust={emotions['disgust']:.1f}")
                last_print_time = time.time()

            raw_scores = np.maximum(np.array([
                emotions['happy'], emotions['neutral'], emotions['surprise'],
                emotions['angry'], emotions['sad'], emotions['fear'], emotions['disgust']
            ]), 0)
            
            if raw_scores.sum() == 0:
                continue

            emotion_probs = softmax(raw_scores)
            new_probs = np.array([
                emotion_probs[0], 
                emotion_probs[1] + emotion_probs[2], 
                sum(emotion_probs[3:7])
            ])
            
            category_probs = (1 - smoothing_factor) * category_probs + smoothing_factor * new_probs
            prob_window.append(category_probs.copy())
            avg_probs = np.mean(prob_window, axis=0) if len(prob_window) == prob_window.maxlen else category_probs

            max_prob = np.max(avg_probs)
            candidate_emotion = categories[np.argmax(avg_probs)]

            if candidate_emotion == current_emotion:
                for e in categories:
                    if e != current_emotion: emotion_counts[e] = 0
            else:
                emotion_counts[candidate_emotion] += 1
                if emotion_counts[candidate_emotion] >= stable_frames_required and max_prob >= min_confidence:
                    current_emotion = candidate_emotion
                    for e in categories: emotion_counts[e] = 0

            with lock:
                emotion_dict['current'] = current_emotion

            no_face_counter = 0

            if current_emotion != previous_emotion:
                time_diff = time.time() - last_change_time
                arrow = "➡️" if current_emotion == "positive" else "⬇️" if current_emotion == "negative" else "⏺️"
                print(f"{arrow} Emotion changed: {previous_emotion.upper()} → {current_emotion.upper()} | Confidence: {max_prob:.2f} | After: {time_diff:.1f}s")
                previous_emotion = current_emotion
                last_change_time = time.time()

            if display:
                cv2.putText(display_frame, f"Emotion: {current_emotion}", (30, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                cv2.putText(display_frame, f"P: {category_probs[0]:.2f} Neu: {category_probs[1]:.2f} Neg: {category_probs[2]:.2f}", (30, 110), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

        except Exception as e:
            no_face_counter += 1
            if no_face_counter % 30 == 0:
                print(f"👤 No face detected for {no_face_counter//30} seconds...")
            if display:
                cv2.putText(display_frame, "NO FACE", (30, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

        if display:
            cv2.imshow("Emotion Detection Debug", display_frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        time.sleep(0.01)

    cap.release()
    if display: cv2.destroyAllWindows()
    print("\n🛑 Emotion module stopped")

if __name__ == "__main__":
    import signal
    manager = multiprocessing.Manager()
    emotion_dict = manager.dict({'current': 'neutral'})
    lock = manager.Lock()
    stop_event = threading.Event()
    signal.signal(signal.SIGINT, lambda sig, frame: stop_event.set())
    run_emotion(stop_event, emotion_dict, lock, display=True)