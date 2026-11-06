#pragma once
// Set up locally before compiling. Do not commit real credentials to GitHub.
// This header is deliberately a template: no Wi-Fi password is assumed.
#define WIFI_SSID "wti-ap-device"
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#define WIFI_PASSWORD ""
#endif

// Existing n8n workflow: Sumanee · Voice Stream.
// Accepts JSON: {"chatInput":"...","sessionId":"cores3-..."}.
// It does not accept raw audio; microphone STT requires a separate bridge.
#define SUMANEE_VOICE_URL "https://sumaneeplatform.witmorning.com/webhook/sumanee-voice-stream-b8f24a91"

// Paste the PEM of the CA (or validated public trust chain) for HTTPS
// requests. Never disable TLS verification in production.
#ifndef VOICE_GATEWAY_CA_CERT
#define VOICE_GATEWAY_CA_CERT ""
#endif

// Separate CoreS3 audio bridge; deploy voice-gateway/ before enabling.
// Set a HTTPS domain with a valid TLS certificate.
#ifndef STACKCHAN_VOICE_BRIDGE_URL
#define STACKCHAN_VOICE_BRIDGE_URL ""
#endif
#if __has_include("voice_secrets.h")
#include "voice_secrets.h"
#else
#define STACKCHAN_VOICE_TOKEN ""
#endif
