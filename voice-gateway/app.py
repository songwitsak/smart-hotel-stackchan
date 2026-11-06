"""CoreS3 voice bridge: WAV microphone -> Thai STT -> existing Sumanee -> WAV TTS."""
import io
import os
import re
import json
import secrets
import wave
import httpx
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import Response
from openai import AsyncOpenAI

app = FastAPI(docs_url=None, redoc_url=None)
client = AsyncOpenAI()
SUMANEE_URL = os.environ.get(
    "SUMANEE_URL",
    "https://sumaneeplatform.witmorning.com/webhook/sumanee-voice-stream-b8f24a91",
)
DEVICE_TOKEN = os.environ.get("DEVICE_TOKEN", "")
MAX_WAV_BYTES = 160_044

def extract_text(payload: str) -> str:
    """Handle plain responses, JSON responses and n8n SSE chunks."""
    pieces = []
    for line in payload.splitlines():
        line = line.strip()
        if line.startswith("data:"):
            line = line[5:].strip()
        if not line or line == "[DONE]":
            continue
        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            pieces.append(line)
            continue
        if isinstance(data, str):
            pieces.append(data)
        elif isinstance(data, dict):
            for key in ("output", "text", "content", "delta"):
                value = data.get(key)
                if isinstance(value, str):
                    pieces.append(value)
                    break
    return "".join(pieces).strip()[:3500]

@app.get("/health")
async def health():
    return {"ok": True}

@app.post("/v1/voice")
async def voice(request: Request):
    if not DEVICE_TOKEN:
        raise HTTPException(503, "Device token not configured")
    auth = request.headers.get("authorization", "")
    if not secrets.compare_digest(auth, "Bearer " + DEVICE_TOKEN):
        raise HTTPException(401, "Unauthorized")
    session_id = request.headers.get("x-session-id", "")
    if not re.fullmatch(r"[A-Za-z0-9_-]{8,100}", session_id):
        raise HTTPException(400, "Invalid session")
    if request.headers.get("content-type", "").split(";")[0].strip() != "audio/wav":
        raise HTTPException(415, "Expected audio/wav")
    wav = await request.body()
    if len(wav) < 100 or len(wav) > MAX_WAV_BYTES:
        raise HTTPException(413, "Audio size must be 100..160044 bytes")
    try:
        with wave.open(io.BytesIO(wav), "rb") as f:
            if f.getnchannels() != 1 or f.getsampwidth() != 2 or f.getframerate() != 16000:
                raise ValueError("Expected 16kHz mono PCM16")
            if f.getnframes() > 80000:
                raise ValueError("Max 5 seconds")
    except (ValueError, wave.Error) as e:
        raise HTTPException(400, str(e))
    try:
        transcript = await client.audio.transcriptions.create(
            model="gpt-4o-mini-transcribe",
            file=("recording.wav", wav, "audio/wav"),
            language="th",
        )
        speech_text = transcript.text.strip()
        if not speech_text:
            raise HTTPException(422, "No speech detected")
        async with httpx.AsyncClient(timeout=75) as http:
            reply = await http.post(
                SUMANEE_URL,
                json={"chatInput": speech_text, "sessionId": session_id},
            )
            reply.raise_for_status()
        answer = extract_text(reply.text)
        if not answer or answer == "Workflow got started.":
            raise HTTPException(502, "Sumanee did not return an AI response")
        # WAV output avoids MP3 decoding on the ESP32.
        tts = await client.audio.speech.create(
            model="tts-1", voice="nova", input=answer[:3500],
            response_format="wav",
        )
        audio = tts.content
        if len(audio) > 900_000:
            raise HTTPException(502, "TTS output too large for CoreS3")
        return Response(
            audio,
            media_type="audio/wav",
            headers={"X-Transcript-Length": str(len(speech_text)),
                     "Cache-Control": "no-store"},
        )
    except HTTPException:
        raise
    except Exception:
        # Do not leak provider details or credentials to devices.
        raise HTTPException(502, "Voice upstream failed")
