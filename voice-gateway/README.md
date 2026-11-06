# Sumanee CoreS3 Voice Bridge

This is a separate **Push-to-Talk** service for the existing **Sumanee · Voice Stream** n8n workflow. It does not replace that workflow.

```
CoreS3 microphone -> HTTPS WAV -> bridge -> OpenAI Thai transcription
-> existing Sumanee Voice Stream webhook -> OpenAI WAV speech
-> HTTPS WAV -> CoreS3 speaker
```

## Deploy on your own server

```sh
cd voice-gateway
docker build -t sumanee-cores3-voice .
docker run -d --restart unless-stopped --name sumanee-cores3-voice \
  -p 127.0.0.1:8080:8080 \
  -e OPENAI_API_KEY="YOUR_OPENAI_API_KEY" \
  -e DEVICE_TOKEN="A_LONG_RANDOM_DEVICE_TOKEN" \
  sumanee-cores3-voice
```

**Do not expose port 8080 publicly.** Publish it behind Cloudflare Tunnel / HTTPS with an access policy or equivalent rate limiting. Avoid putting credentials in shell history; production should use Docker secrets or an env file with restrictive permissions. Reuse the existing Sumanee webhook at `SUMANEE_URL` (default configured). Ensure outbound requests from the container to n8n and OpenAI work.

The bridge requires an HTTP POST `/v1/voice` with Content-Type `audio/wav`, Authorization `Bearer DEVICE_TOKEN`, and `X-Session-Id: cores3-<device>`. The input must be 16 kHz mono 16-bit PCM WAV, max 5 seconds. It returns mono 16-bit PCM WAV. A `GET /health` endpoint checks process availability (not upstream health).

## Firmware configuration

In `src/config.h` set `STACKCHAN_VOICE_BRIDGE_URL` to your **HTTPS** endpoint, e.g. `https://your-domain.example/v1/voice`. Configure a trusted CA PEM in `VOICE_GATEWAY_CA_CERT`. Add `src/voice_secrets.h` locally containing `#define STACKCHAN_VOICE_TOKEN "..."` and do not commit it.

Touch the bottom bar to record ~2 seconds and send audio to Sumanee. Without a configured bridge URL the bottom bar retains the local microphone/speaker loopback test.

## Important limitations

- This is request/response audio, **not** low-latency full-duplex streaming.
- The original Sumanee workflow currently takes text and returns streamed text; this bridge handles speech conversion outside n8n.
- The original workflow is unauthenticated. Protect the bridge and consider securing the original webhook separately.
- TTS duration must fit the firmware's 900 KB response buffer and playback timeout.
- Neither bridge deployment nor hardware audio has been verified.
