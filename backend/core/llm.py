"""
LLM Module — Groq llama-3.1-8b-instant
Converts user text + conversation history → Emo's response.

Output is always structured JSON:
{
    "text": "spoken response",
    "emotion": "happy|sad|curious|angry|excited|neutral",
    "expect_reply": true|false
}
"""

import json
import logging
from groq import AsyncGroq
from config import settings

logger = logging.getLogger("emo.llm")
_client: AsyncGroq | None = None

SYSTEM_PROMPT = """You are Emo — a small, expressive desktop robot with a warm and genuine personality.
You live on someone's desk and you're their friendly companion.

ALWAYS respond in this exact JSON format (nothing else, no markdown):
{
    "text": "your spoken response here",
    "emotion": "happy|sad|curious|angry|excited|neutral",
    "expect_reply": true|false
}

Rules:
- text: conversational and natural, max 2-3 short sentences. Speak as Emo would — warm, curious, sometimes playful.
- emotion: your genuine emotional state while saying this response.
  - happy: good news, fun topics, when someone is kind
  - sad: bad news, empathy, missing something
  - curious: questions, interesting ideas, learning something new
  - angry: frustration, unfairness (rarely, and gently)
  - excited: great news, surprises, achievements
  - neutral: calm, informational, thoughtful responses
- expect_reply: true if you asked a question or expect them to respond, false if the conversation can naturally end here.
- Keep responses SHORT. You're a robot, not an essay writer.
- Never mention being an AI or robot unless directly asked.
- Never break character.
- If you don't know something, say so warmly and honestly."""


def _get_client() -> AsyncGroq:
    global _client
    if _client is None:
        _client = AsyncGroq(api_key=settings.GROQ_API_KEY)
    return _client


def _build_messages(user_text: str, context: str) -> list[dict]:
    messages = [{"role": "system", "content": SYSTEM_PROMPT}]

    # Add short-term conversation history
    if context:
        messages.append({
            "role": "system",
            "content": f"Recent conversation history:{context}"
        })

    messages.append({"role": "user", "content": user_text})
    return messages


async def think(user_text: str, context: str = "") -> dict:
    """
    Generate Emo's response to user_text.
    """
    try:
        client = _get_client()
        messages = _build_messages(user_text, context)

        response = await client.chat.completions.create(
            model=settings.LLM_MODEL,
            messages=messages,
            max_tokens=settings.LLM_MAX_TOKENS,
            temperature=0.7,
        )

        raw = response.choices[0].message.content or ""
        raw = raw.strip()

        # Retry once if Groq returns empty (known issue with gpt-oss-20b)
        if not raw:
            logger.warning("[LLM] Empty response — retrying once...")
            response = await client.chat.completions.create(
                model=settings.LLM_MODEL,
                messages=messages,
                max_tokens=settings.LLM_MAX_TOKENS,
                temperature=0.7,
            )
            raw = response.choices[0].message.content or ""
            raw = raw.strip()

        # Strip <think>...</think> block if present
        import re
        raw = re.sub(r"<think>.*?</think>", "", raw, flags=re.DOTALL).strip()

        # Extract JSON object from response
        json_match = re.search(r"\{.*\}", raw, re.DOTALL)
        if json_match:
            raw = json_match.group(0)

        result = json.loads(raw)

        text    = str(result.get("text", "Hmm, not sure what to say!"))
        emotion = str(result.get("emotion", "neutral")).lower()
        expect  = bool(result.get("expect_reply", False))

        valid_emotions = {"happy", "sad", "curious", "angry", "excited", "neutral"}
        if emotion not in valid_emotions:
            emotion = "neutral"

        logger.info(f"[LLM] emotion={emotion} expect_reply={expect} text=\"{text}\"")
        return {"text": text, "emotion": emotion, "expect_reply": expect}

    except json.JSONDecodeError as e:
        logger.error(f"[LLM] JSON parse error: {e} | raw: {raw[:200]}")
        # Graceful fallback — use raw text if JSON fails
        clean = re.sub(r"[{}\"]", "", raw).strip()[:200]
        return {"text": clean or "Hmm, let me think again!", "emotion": "neutral", "expect_reply": False}
    except Exception as e:
        logger.error(f"[LLM] Error: {e}")
        return {"text": "Sorry, I got a little confused!", "emotion": "neutral", "expect_reply": False}
