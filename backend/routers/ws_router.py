"""
WebSocket Router — ESP32 ↔ Backend
Handles all real-time communication with the ESP32.
ws_lock is shared between the receive loop AND pipeline tasks so all
websocket.send_* calls are serialized — no frame interleaving possible.
"""

import asyncio
import json
import logging
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from core.session import Session, SessionManager
from core import pipeline

logger = logging.getLogger("emo.ws")
router = APIRouter()

session_manager = SessionManager()


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    client_ip = websocket.client.host
    device_id = client_ip

    logger.info(f"🔌 ESP32 connected from {client_ip}")
    session = session_manager.get_or_create(device_id)

    # Single lock shared between receive loop AND all pipeline tasks.
    # Guarantees all websocket.send_* calls are serialized — prevents
    # frame corruption from concurrent pong + audio chunk sends.
    ws_lock = asyncio.Lock()

    async def safe_send_json(data: dict) -> None:
        try:
            async with ws_lock:
                await websocket.send_json(data)
        except Exception as e:
            logger.warning(f"Error sending json: {e}")

    await safe_send_json({
        "type": "connected",
        "message": "Emo backend ready"
    })

    # Track active turn task so only one turn runs at a time
    current_turn_task: asyncio.Task | None = None

    try:
        while True:
            # Receive text or binary
            message = await websocket.receive()

            # ── Binary: raw audio from ESP32 mic ──────────────
            if "bytes" in message and message["bytes"]:
                audio = message["bytes"]
                logger.info(f"🎙️ Audio received: {len(audio)} bytes")
                is_wav = audio[:4] == b"RIFF"

                if current_turn_task and not current_turn_task.done():
                    logger.warning("Turn already in progress — ignoring new audio")
                    continue

                current_turn_task = asyncio.create_task(
                    pipeline.run(websocket, session, audio, is_wav=is_wav, ws_lock=ws_lock)
                )
                continue

            # ── Text: JSON control messages ────────────────────
            if "text" not in message or not message["text"]:
                continue

            try:
                msg = json.loads(message["text"])
            except json.JSONDecodeError:
                logger.warning(f"⚠️ Invalid JSON from {client_ip}")
                continue

            msg_type = msg.get("type", "unknown")

            # ── Ping / keepalive ───────────────────────────────
            if msg_type == "ping":
                await safe_send_json({"type": "pong"})

            # ── Wake word detected on ESP32 ────────────────────
            elif msg_type == "wake_detected":
                logger.info("🎙️ Wake word detected by ESP32")
                session.state = "listening"
                await safe_send_json({
                    "type": "state",
                    "state": "listening"
                })

            # ── Text input (Serial Monitor testing) ───────────
            elif msg_type == "text_input":
                text = msg.get("text", "").strip()
                if text:
                    logger.info(f"💬 Text input: \"{text}\"")
                    # Never await a turn here: blocking the receive loop would
                    # starve pings/pongs and make the ESP32 drop the connection.
                    if current_turn_task and not current_turn_task.done():
                        logger.warning("Turn already in progress — ignoring new text input")
                        continue

                    current_turn_task = asyncio.create_task(
                        pipeline.run_text(websocket, session, text, ws_lock=ws_lock)
                    )

            # ── Audio chunk from ESP32 (base64 fallback) ───────
            elif msg_type == "audio":
                audio_b64 = msg.get("data", "")
                if audio_b64:
                    if current_turn_task and not current_turn_task.done():
                        logger.warning("Turn already in progress — ignoring new audio")
                        continue
                    import base64
                    audio_bytes = base64.b64decode(audio_b64)
                    current_turn_task = asyncio.create_task(
                        pipeline.run(websocket, session, audio_bytes, ws_lock=ws_lock)
                    )

            # ── Status query ───────────────────────────────────
            elif msg_type == "status":
                await safe_send_json({
                    "type": "status",
                    "state": session.state,
                    "sessions": session_manager.active_count()
                })

            else:
                logger.warning(f"⚠️ Unknown message type: {msg_type}")

    except WebSocketDisconnect:
        logger.info(f"🔌 ESP32 disconnected ({client_ip})")
        if current_turn_task and not current_turn_task.done():
            current_turn_task.cancel()
        session_manager.remove(device_id)
    except Exception as e:
        logger.error(f"❌ WebSocket error: {e}", exc_info=True)
        if current_turn_task and not current_turn_task.done():
            current_turn_task.cancel()
        session_manager.remove(device_id)
