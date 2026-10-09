"""
STT Module — Groq Whisper
Converts raw audio bytes → text transcript.
Input: WAV bytes (16kHz, 16-bit, mono) from ESP32
"""

import io
import wave
import logging
from groq import AsyncGroq
from config import settings

logger = logging.getLogger("emo.stt")
_client: AsyncGroq | None = None


def _get_client() -> AsyncGroq:
    global _client
    if _client is None:
        _client = AsyncGroq(api_key=settings.GROQ_API_KEY)
    return _client


def _pcm_to_wav(pcm_bytes: bytes, sample_rate: int = 16000,
                channels: int = 1, sampwidth: int = 2) -> bytes:
    """Wrap raw PCM bytes in a WAV container for Groq API."""
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(sampwidth)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm_bytes)
    return buf.getvalue()


async def transcribe(audio_bytes: bytes, is_wav: bool = False) -> str:
    """
    Transcribe audio bytes to text using Groq Whisper.

    Args:
        audio_bytes: Raw PCM or WAV bytes
        is_wav: True if audio_bytes already has WAV header

    Returns:
        Transcribed text string, or "" on failure
    """
    try:
        wav_bytes = audio_bytes if is_wav else _pcm_to_wav(audio_bytes)

        client = _get_client()
        transcription = await client.audio.transcriptions.create(
            file=("audio.wav", wav_bytes, "audio/wav"),
            model=settings.STT_MODEL,
            language="en",
            response_format="text"
        )

        text = str(transcription).strip()
        logger.info(f"[STT] Transcript: \"{text}\"")
        return text

    except Exception as e:
        logger.error(f"[STT] Error: {e}")
        return ""
