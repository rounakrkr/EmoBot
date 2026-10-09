from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
import json
import logging
from contextlib import asynccontextmanager

from config import settings
from routers import ws_router
from core.session import SessionManager

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("emo")


@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("🤖 Emo Backend starting up...")
    logger.info(f"   LLM  : {settings.LLM_PROVIDER} / {settings.LLM_MODEL}")
    logger.info(f"   STT  : {settings.STT_PROVIDER}")
    logger.info(f"   TTS  : {settings.TTS_PROVIDER}")
    yield
    logger.info("🛑 Emo Backend shutting down...")


app = FastAPI(
    title="Emo Robot Backend",
    version="2.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(ws_router.router)


@app.get("/health")
async def health():
    return {
        "status": "ok",
        "robot": "Emo",
        "version": "2.0.0"
    }


if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host=settings.BACKEND_HOST,
        port=settings.BACKEND_PORT,
        reload=False
    )
