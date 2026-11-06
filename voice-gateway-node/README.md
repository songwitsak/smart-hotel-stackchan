# Windows + npm: CoreS3 ↔ Existing Sumanee Voice

Uses **the existing Sumanee · Voice Stream n8n workflow**, including its memory. No second AI Agent or n8n installation.

## Requirements
- Windows with Node.js **20+** (`node -v`).
- Existing n8n running separately via npm.
- OpenAI API key with access to `gpt-4o-mini-transcribe` and `tts-1`.
- HTTPS reverse proxy / Cloudflare Tunnel routing a **new hostname** to `http://127.0.0.1:8080`.
- Do **not** route the new hostname over your existing n8n hostname; keep its current routes intact.

## Start in PowerShell
```powershell
cd voice-gateway-node
$env:OPENAI_API_KEY = "YOUR_OPENAI_API_KEY"
$env:DEVICE_TOKEN = "YOUR_LONG_RANDOM_DEVICE_TOKEN"
$env:PORT = "8080"
npm start
```
For production, store secrets securely and run as a managed Windows service. Do not commit credentials. Keep the local HTTP listener bound to 127.0.0.1; publish only through HTTPS with rate limiting and access controls.

```powershell
Invoke-RestMethod http://127.0.0.1:8080/health
```

## CoreS3
Configure `STACKCHAN_VOICE_BRIDGE_URL` with your new HTTPS hostname plus `/v1/voice`, set the **same** `STACKCHAN_VOICE_TOKEN` in `src/voice_secrets.h`, and supply a trusted PEM CA certificate in `VOICE_GATEWAY_CA_CERT`. Build and flash. Touch the bottom bar to record and talk.

## Notes
- WAV 16kHz PCM16 mono, up to 5 seconds. Firmware currently records 2 seconds.
- Audio is not streaming or full duplex; there is a wait while STT, Sumanee and TTS complete.
- `GET /health` confirms this Node.js process only, not upstream services.
- Sumanee's existing workflow accepts text; this service adds audio conversion.
- This service has **not been deployed or tested on your Windows host**.
