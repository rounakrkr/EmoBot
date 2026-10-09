import os
import time
import queue
import threading
import requests
from dotenv import load_dotenv

from memory import ConversationMemory
from cloud_memory import CloudMemory

# Load environment variables
load_dotenv()

# --- Configuration ---
MODE = "online"  # "offline" | "online" | "hybrid"
GROQ_API_KEY = os.getenv("GROQ_API_KEY")
GROQ_URL = "https://api.groq.com/openai/v1/chat/completions"

LOCAL_OLLAMA_URL = "http://localhost:11434/api/generate"
LOCAL_MODEL = "phi3:latest"

# Cloud memory will automatically use JSONBIN_BIN_ID and JSONBIN_API_KEY from .env
cloud_mem = CloudMemory()

# --- Global State ---
running = False
input_queue = None
tts_queue = None
MEMORY_SIZE = 5
_esp = None

conversation_memory = ConversationMemory(max_pairs=MEMORY_SIZE)

def internet_available():
    try:
        requests.get("https://www.google.com", timeout=0.5)
        return True
    except requests.RequestException:
        return False

def build_prompt(user_text, emotion):
    memory_context = conversation_memory.get_context()
    
    return f"""You are a small desk robot. Your name is Aino.
You are the user's companion – you chat with them, answer questions, and understand their feelings.

IMPORTANT – RESPONSE PRIORITY:
1️⃣ FIRST: Answer the user's query directly and helpfully (70-80% of reply)
2️⃣ SECOND: If relevant, briefly acknowledge their detected emotion ({emotion}) at the end
   * positive → light, cheerful acknowledgment
   * neutral → no need to mention unless relevant
   * negative → brief supportive comment at the end

EMOTION HANDLING:
- User's visible emotion from face is: {emotion}
- If the user's words match their emotion, you can add a quick acknowledgment after answering.
- If there's a mismatch, you can gently point it out, but only after answering their question.
- Do NOT let emotion handling dominate the reply.

PUNCTUATION GUIDELINES:
- Use normal punctuation: . , ? !
- AVOID: – — ... (em dash, en dash, ellipsis)
- Use commas naturally, end sentences properly.

{memory_context}

User said: {user_text}
User's visible emotion: {emotion}

Reply in 2-3 short English sentences. First answer the question, then optionally acknowledge the emotion. Be natural and friendly."""

def call_groq(prompt):
    headers = {
        "Authorization": f"Bearer {GROQ_API_KEY}",
        "Content-Type": "application/json"
    }
    data = {
        "model": "llama-3.1-8b-instant",
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": 80,
        "temperature": 0.5
    }
    try:
        r = requests.post(GROQ_URL, headers=headers, json=data, timeout=3)
        r.raise_for_status()
        return r.json()['choices'][0]['message']['content'].strip()
    except Exception as e:
        print(f"⚠️ Groq error: {e}")
        return None

def call_local(prompt):
    payload = {
        "model": LOCAL_MODEL,
        "prompt": prompt,
        "stream": False,
        "options": {"temperature": 0.5, "max_tokens": 80},
        "keep_alive": "10m"
    }
    try:
        r = requests.post(LOCAL_OLLAMA_URL, json=payload, timeout=10)
        r.raise_for_status()
        return r.json()["response"].strip()
    except Exception as e:
        print(f"⚠️ Ollama error: {e}")
        return None

def process_memory_commands(text_lower):
    """Handles cloud memory explicit commands. Returns response if handled, else None."""
    if "remember my" in text_lower:
        parts = text_lower.replace("remember my", "").split(" is ")
        if len(parts) == 2:
            key, value = parts[0].strip(), parts[1].strip()
            if cloud_mem.remember(key, value):
                return f"Okay, I'll remember that your {key} is {value}."
            return "Sorry, I couldn't save that to cloud right now."
            
    if "what is my" in text_lower:
        key = text_lower.replace("what is my", "").strip()
        value = cloud_mem.recall(key)
        return f"Your {key} is {value}." if value else f"I don't have anything saved for {key}."
        
    if "forget my" in text_lower:
        key = text_lower.replace("forget my", "").strip()
        return f"I've forgotten your {key}." if cloud_mem.forget(key) else f"I don't have anything saved for {key}."
        
    if "what do you remember" in text_lower or "what do you know" in text_lower:
        all_mem = cloud_mem.get_all()
        if all_mem:
            facts = "\n".join([f"• Your {k} is {v['value']}" for k, v in all_mem.items()])
            return f"Here's what I remember about you:\n{facts}"
        return "I don't remember anything about you yet. You can tell me things like 'remember my favorite color is blue'."
    
    return None

def generate_reply(user_text, emotion):
    # Check for memory commands first
    mem_response = process_memory_commands(user_text.lower())
    if mem_response:
        return mem_response

    prompt = build_prompt(user_text, emotion)

    if MODE == "offline":
        return call_local(prompt) or "I'm having trouble thinking right now."
    elif MODE == "online":
        return call_groq(prompt) or "I'm having trouble connecting."
    elif MODE == "hybrid":
        if internet_available():
            reply = call_groq(prompt)
            if reply: return reply
            print("⚠️ Online failed, falling back to offline...")
        return call_local(prompt) or "I'm having trouble responding."

def llm_worker():
    global running
    print("🧠 LLM worker started")
    while running:
        try:
            item = input_queue.get(timeout=0.5)
        except queue.Empty:
            continue
        
        if item is None:
            continue
            
        try:
            user_text, emotion = item
            print(f"🧠 LLM received: '{user_text}' (emotion: {emotion})")
            
            reply = generate_reply(user_text, emotion)

            if _esp:
                if emotion == "positive": _esp.positive()
                elif emotion == "neutral": _esp.neutral()
                elif emotion == "negative": _esp.negative()
            
            conversation_memory.add_interaction(user_text, reply)
            
            reply = reply.replace("\n", " ")
            if not reply.endswith((".", "!", "?")):
                reply += "..."
                
            print(f"🤖 AI: {reply}")
            if tts_queue is not None:
                tts_queue.put_nowait((reply, emotion))
                
        except Exception as e:
            print(f"❌ LLM worker error: {e}")
            
    print("🛑 LLM worker exiting")

def start(llm_input_queue, tts_output_queue=None, esp=None):
    global running, input_queue, tts_queue, _esp
    input_queue = llm_input_queue
    tts_queue = tts_output_queue
    _esp = esp
    running = True
    
    thread = threading.Thread(target=llm_worker, daemon=True)
    thread.start()

    if MODE in ["offline", "hybrid"]:
        def warmup():
            time.sleep(2)
            try:
                requests.post(LOCAL_OLLAMA_URL, 
                            json={"model": LOCAL_MODEL, "prompt": "Hi", "stream": False, "keep_alive": "10m"},
                            timeout=5)
                print("✅ Ollama warmed up")
            except Exception as e:
                print(f"⚠️ Ollama warm-up failed: {e}")
        threading.Thread(target=warmup, daemon=True).start()
    
    print(f"✅ LLM module started (mode: {MODE}, model: {LOCAL_MODEL})")
    return thread

def stop():
    global running
    running = False
    print("🛑 LLM module stopped")

def send_to_llm(text, emotion="neutral"):
    if input_queue:
        input_queue.put((text, emotion))