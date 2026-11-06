#pragma once
// Set up locally before compiling. Do not commit real credentials to GitHub.
// This header is deliberately a template: no Wi-Fi password is assumed.
#define WIFI_SSID "wti-ap-device"
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#define WIFI_PASSWORD ""
#endif

// Existing text-only Sumanee Voice Stream (web client).
#define SUMANEE_VOICE_URL "https://sumaneeplatform.witmorning.com/webhook/sumanee-voice-stream-b8f24a91"

// Direct WAV webhook in the SAME n8n workflow.
// Currently responds with JSON text; WAV speech response is not enabled yet.
#define SUMANEE_CORES3_WAV_URL "https://sumaneeplatform.witmorning.com/webhook/sumanee-cores3-wav"

// GitHub Actions generates a Mozilla-derived CA bundle at build time.
// Local builds can supply their own PEM using voice_ca_bundle.h.
#if __has_include("voice_ca_bundle.h")
#include "voice_ca_bundle.h"
#else
#ifndef VOICE_GATEWAY_CA_CERT
#define VOICE_GATEWAY_CA_CERT ""
#endif
#endif

// Optional legacy audio bridge. Leave empty to send WAV directly to n8n.
// Set a HTTPS domain with a valid TLS certificate.
#ifndef STACKCHAN_VOICE_BRIDGE_URL
#define STACKCHAN_VOICE_BRIDGE_URL ""
#endif
#if __has_include("voice_secrets.h")
#include "voice_secrets.h"
#else
#define STACKCHAN_VOICE_TOKEN ""
#endif
