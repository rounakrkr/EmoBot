"""
Pipeline — STT → LLM → TTS
Orchestrates the full conversation turn for Emo.
Audio is sent as paced chunked raw PCM (22050 Hz, 16-bit, mono) over WebSocket.

ws_lock must be passed in to serialize all WebSocket sends and prevent
frame interleaving between the pipeline task and the pong handler.
"""

import asyncio
import logging
import time
from fastapi import WebSocket
from core.stt import transcribe
from core.llm import think
from core.tts import synthesize
from core.session import Session

logger = logging.getLogger("emo.pipeline")

# 1024 bytes = 512 int16 samples = ~23.2ms of audio at 22050 Hz
AUDIO_CHUNK_SIZE = 1024


async def _send_audio(websocket: WebSocket, pcm_bytes: bytes, ws_lock: asyncio.Lock) -> None:
    """
    Stream raw PCM audio to ESP32 as paced 1024-byte binary WebSocket frames.
    All sends go through ws_lock to prevent frame interleaving with pong replies.

    sleep(0.012) is NOT TOUCHED — instrumentation will reveal actual pacing.
    """
    if not pcm_bytes:
        return

    total_len = len(pcm_bytes)
    total_chunks = (total_len + AUDIO_CHUNK_SIZE - 1) // AUDIO_CHUNK_SIZE

    logger.info(f"[PIPELINE] Streaming {total_len} bytes PCM ({total_chunks} chunks)...")

    async with ws_lock:
        await websocket.send_json({
            "type": "audio_start",
            "total_bytes": total_len,
            "sample_rate": 22050,
            "channels": 1,
            "bits": 16,
        })

    # Small pause so ESP32 resets buffer
    await asyncio.sleep(0.04)

    t_start = time.perf_counter()

    for idx, i in enumerate(range(0, total_len, AUDIO_CHUNK_SIZE)):
        chunk = pcm_bytes[i:i + AUDIO_CHUNK_SIZE]
        async with ws_lock:
            await websocket.send_bytes(chunk)

        # Pacing instrumentation: log every 20th chunk
        if idx > 0 and idx % 20 == 0:
            elapsed = time.perf_counter() - t_start
            sent_bytes = (idx + 1) * AUDIO_CHUNK_SIZE
            rate_kbps = sent_bytes / elapsed / 1024 if elapsed > 0 else 0
            logger.info(f"[PACING] chunk {idx}/{total_chunks} @ {elapsed:.3f}s, "
                        f"rate={rate_kbps:.1f} KB/s")

        # Pre-buffer first 4 chunks with minimal delay, then pace at ~12ms
        if idx < 4:
            await asyncio.sleep(0.002)
        else:
            await asyncio.sleep(0.012)

    t_total = time.perf_counter() - t_start
    effective_rate = total_len / t_total / 1024 if t_total > 0 else 0
    audio_duration_ms = total_len / (22050 * 2) * 1000
    logger.info(f"[PACING] DONE: {total_chunks} chunks in {t_total:.3f}s, "
                f"effective={effective_rate:.1f} KB/s, "
                f"audio_duration={audio_duration_ms:.0f}ms, "
                f"delivery_ratio={t_total / (audio_duration_ms / 1000):.2f}x realtime")

    await asyncio.sleep(0.04)
    async with ws_lock:
        await websocket.send_json({"type": "audio_end"})
    logger.info(f"[PIPELINE] ✓ Finished streaming all {total_chunks} chunks ({total_len} bytes)")


async def run(
    websocket: WebSocket,
    session: Session,
    audio_bytes: bytes,
    is_wav: bool = False,
    ws_lock: asyncio.Lock = None
) -> None:
    """
    Full pipeline for one conversation turn.
    """
    if ws_lock is None:
        ws_lock = asyncio.Lock()

    # ── Step 1: STT ─────────────────────────────────────
    async with ws_lock:
        await websocket.send_json({"type": "state", "state": "thinking"})
    logger.info("[PIPELINE] Running STT...")

    transcript = await transcribe(audio_bytes, is_wav=is_wav)

    if not transcript:
        logger.warning("[PIPELINE] Empty transcript — aborting")
        async with ws_lock:
            await websocket.send_json({
                "type": "error",
                "message": "Could not understand audio"
            })
            await websocket.send_json({"type": "state", "state": "idle"})
        return

    logger.info(f"[PIPELINE] User said: \"{transcript}\"")
    async with ws_lock:
        await websocket.send_json({"type": "transcript", "text": transcript})

    # ── Step 2: LLM ─────────────────────────────────────
    logger.info("[PIPELINE] Running LLM...")
    context = session.get_context()
    response = await think(transcript, context)

    text         = response["text"]
    emotion      = response["emotion"]
    expect_reply = response["expect_reply"]

    session.add_turn(transcript, text)

    # ── Step 3: TTS ─────────────────────────────────────
    async with ws_lock:
        await websocket.send_json({"type": "state", "state": "speaking"})
    logger.info("[PIPELINE] Running TTS...")
    pcm_bytes = await synthesize(text)

    # ── Step 4: Send response to ESP32 ──────────────────
    async with ws_lock:
        await websocket.send_json({
            "type": "response",
            "text": text,
            "emotion": emotion,
            "expect_reply": expect_reply,
        })

    await _send_audio(websocket, pcm_bytes, ws_lock)

    async with ws_lock:
        await websocket.send_json({"type": "state", "state": "idle"})
    logger.info(f"[PIPELINE] Turn complete — emotion={emotion}, expect_reply={expect_reply}")


async def run_text(
    websocket: WebSocket,
    session: Session,
    text: str,
    ws_lock: asyncio.Lock = None
) -> None:
    """
    Pipeline starting from text (skip STT).
    Used for Serial Monitor input while mic is not connected.
    """
    if ws_lock is None:
        ws_lock = asyncio.Lock()

    logger.info(f"[PIPELINE/TEXT] Input: \"{text}\"")
    async with ws_lock:
        await websocket.send_json({"type": "state", "state": "thinking"})
        await websocket.send_json({"type": "transcript", "text": text})

    context  = session.get_context()
    response = await think(text, context)

    text_out     = response["text"]
    emotion      = response["emotion"]
    expect_reply = response["expect_reply"]

    session.add_turn(text, text_out)

    async with ws_lock:
        await websocket.send_json({"type": "state", "state": "speaking"})
    pcm_bytes = await synthesize(text_out)

    async with ws_lock:
        await websocket.send_json({
            "type": "response",
            "text": text_out,
            "emotion": emotion,
            "expect_reply": expect_reply,
        })

    await _send_audio(websocket, pcm_bytes, ws_lock)

    async with ws_lock:
        await websocket.send_json({"type": "state", "state": "idle"})
