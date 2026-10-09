from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    # APIs
    GROQ_API_KEY: str = ""
    GEMINI_API_KEY: str = ""

    # LLM
    LLM_PROVIDER: str = "groq"
    LLM_MODEL: str = "openai/gpt-oss-20b"
    LLM_MAX_TOKENS: int = 500

    # STT
    STT_PROVIDER: str = "groq"
    STT_MODEL: str = "whisper-large-v3-turbo"

    # TTS
    TTS_PROVIDER: str = "edge_tts"
    TTS_VOICE: str = "en-US-AnaNeural"

    # Memory
    SLIDING_WINDOW_SIZE: int = 5   # number of recent turns kept as context
    MEMORY_PERSIST_ACROSS_SESSIONS: bool = True

    # Server
    BACKEND_HOST: str = "0.0.0.0"
    BACKEND_PORT: int = 8001

    # Attention window
    ATTENTION_TIMEOUT_SECONDS: int = 20

    # Debug
    LOG_LEVEL: str = "INFO"

    class Config:
        env_file = ".env"
        extra = "ignore"


settings = Settings()
