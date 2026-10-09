import os
import io
import wave
import numpy as np
from groq import Groq
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

class GroqSTT:
    def __init__(self):
        self.client = Groq(api_key=os.getenv("GROQ_API_KEY"))
        self.model = "whisper-large-v3-turbo"
    
    def transcribe(self, audio_np, sample_rate=16000, language="en"):
        try:
            wav_buffer = io.BytesIO()
            with wave.open(wav_buffer, 'wb') as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(sample_rate)
                wf.writeframes((audio_np * 32767).astype(np.int16).tobytes())
            
            wav_buffer.seek(0)
            
            transcription = self.client.audio.transcriptions.create(
                file=("audio.wav", wav_buffer.read()),
                model=self.model,
                response_format="text",
                language=language
            )
            return transcription
        except Exception as e:
            print(f"❌ Groq STT error: {e}")
            return None

# Global instance
groq_stt = GroqSTT()