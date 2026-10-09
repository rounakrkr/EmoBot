"""
Session Manager — tracks per-device conversation state.
Each ESP32 (by its WebSocket connection) gets its own session
with short-term memory (sliding window).
"""

import time
import logging
from collections import deque
from dataclasses import dataclass, field
from typing import Optional

from config import settings

logger = logging.getLogger("emo.session")


@dataclass
class Session:
    device_id: str
    created_at: float = field(default_factory=time.time)
    last_active: float = field(default_factory=time.time)
    short_term: deque = field(default_factory=lambda: deque(maxlen=settings.SLIDING_WINDOW_SIZE))
    current_emotion: str = "neutral"
    state: str = "idle"   # idle | listening | thinking | speaking

    def add_turn(self, user_text: str, bot_text: str):
        self.short_term.append({
            "user": user_text,
            "bot": bot_text,
            "ts": time.time()
        })
        self.last_active = time.time()

    def get_context(self) -> str:
        if not self.short_term:
            return ""
        lines = ["\nPrevious conversation:"]
        for turn in self.short_term:
            lines.append(f"User: {turn['user']}")
            lines.append(f"Emo: {turn['bot']}")
        return "\n".join(lines)

    def touch(self):
        self.last_active = time.time()


class SessionManager:
    def __init__(self):
        self._sessions: dict[str, Session] = {}

    def get_or_create(self, device_id: str) -> Session:
        if device_id not in self._sessions:
            self._sessions[device_id] = Session(device_id=device_id)
            logger.info(f"📋 New session created for {device_id}")
        else:
            self._sessions[device_id].touch()
        return self._sessions[device_id]

    def remove(self, device_id: str):
        if device_id in self._sessions:
            del self._sessions[device_id]
            logger.info(f"📋 Session removed for {device_id}")

    def active_count(self) -> int:
        return len(self._sessions)
