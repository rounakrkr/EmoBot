"""
TTS Module — Edge TTS
Converts text → raw PCM audio bytes (22050 Hz, 16-bit, mono).

Flow:
  edge_tts → MP3 bytes
  ffmpeg   → raw PCM at 22050 Hz, 16-bit, mono
  pipeline → 4KB binary chunks to ESP32

ESP32 does 2× sample duplication → 44100 Hz stereo for A2DP speaker.
"""

import io
import asyncio
import logging
import subprocess
import edge_tts
from config import settings

logger = logging.getLogger("emo.tts")


def _mp3_to_pcm22050(mp3_bytes: bytes) -> bytes:
    """
    Decode MP3 → raw PCM at 22050 Hz, 16-bit, mono using ffmpeg.
    ffmpeg must be installed on server (apt-get install ffmpeg).
    """
    cmd = [
        "ffmpeg",
        "-hide_banner", "-loglevel", "error",
        "-i", "pipe:0",            # read MP3 from stdin
        "-f", "s16le",             # raw signed 16-bit little-endian PCM
        "-ar", "22050",            # 22050 Hz sample rate
        "-ac", "1",                # mono
        "pipe:1",                  # write to stdout
    ]
    try:
        result = subprocess.run(
            cmd,
            input=mp3_bytes,
            capture_output=True,
            timeout=30,
        )
        if result.returncode != 0:
            logger.error(f"[TTS/ffmpeg] Error: {result.stderr.decode()[:200]}")
            return b""
        return result.stdout
    except FileNotFoundError:
        logger.error("[TTS/ffmpeg] ffmpeg not found — install with: sudo apt-get install ffmpeg")
        return b""
    except subprocess.TimeoutExpired:
        logger.error("[TTS/ffmpeg] Timeout")
        return b""


async def synthesize(text: str) -> bytes:
    """
    Convert text to raw PCM audio bytes (22050 Hz, 16-bit, mono).

    Args:
        text: The text Emo should speak

    Returns:
        Raw PCM bytes, or empty bytes on failure
    """
    try:
        # Step 1: edge_tts → MP3
        communicate = edge_tts.Communicate(text, voice=settings.TTS_VOICE)
        buf = io.BytesIO()
        async for chunk in communicate.stream():
            if chunk["type"] == "audio":
                buf.write(chunk["data"])
        mp3_bytes = buf.getvalue()

        if not mp3_bytes:
            logger.error("[TTS] edge_tts returned no audio")
            return b""

        logger.info(f"[TTS] Got {len(mp3_bytes)} bytes MP3 — converting to PCM...")

        # Step 2: ffmpeg → raw PCM 22050 Hz mono (run in thread to not block event loop)
        loop = asyncio.get_event_loop()
        pcm_bytes = await loop.run_in_executor(None, _mp3_to_pcm22050, mp3_bytes)

        if pcm_bytes:
            duration_ms = int(len(pcm_bytes) / (22050 * 2) * 1000)
            logger.info(f"[TTS] PCM: {len(pcm_bytes)} bytes (~{duration_ms}ms)")

        return pcm_bytes

    except Exception as e:
        logger.error(f"[TTS] Error: {e}")
        return b""
