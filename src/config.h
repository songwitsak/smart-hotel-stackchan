#pragma once
// Set up locally before compiling. Do not commit real credentials to GitHub.
// This header is deliberately a template: no Wi-Fi password is assumed.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Existing n8n workflow: Sumanee · Voice Stream.
// Accepts JSON: {"chatInput":"...","sessionId":"cores3-..."}.
// It does not accept raw audio; microphone STT requires a separate bridge.
#define SUMANEE_VOICE_URL "https://sumaneeplatform.witmorning.com/webhook/sumanee-voice-stream-b8f24a91"

// Paste the PEM of the CA (or validated public trust chain) for HTTPS
// requests. Never disable TLS verification in production.
#define VOICE_GATEWAY_CA_CERT ""
