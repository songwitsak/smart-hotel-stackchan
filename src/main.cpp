#include <M5Unified.h>
#include <ESP32Servo.h>
#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <HTTPClient.h>
#include "config.h"
#if __has_include("portrait_asset.h")
#include "portrait_asset.h"
#define HAS_SUMANEE_PORTRAIT 1
#else
#define HAS_SUMANEE_PORTRAIT 0
#endif
#include <esp_heap_caps.h>

// M5Stack CoreS3 / StackChan Phase 1: face + IMU + optional head servo.
// IMPORTANT: The servo pin is DISABLED by default. Confirm your StackChan
// base wiring and power before setting STACKCHAN_SERVO_PIN in platformio.ini.
#ifndef STACKCHAN_SERVO_PIN
#define STACKCHAN_SERVO_PIN -1
#endif
#ifndef STACKCHAN_SERVO_MIN_DEG
#define STACKCHAN_SERVO_MIN_DEG 65
#endif
#ifndef STACKCHAN_SERVO_MAX_DEG
#define STACKCHAN_SERVO_MAX_DEG 115
#endif

Servo headServo;
bool servoReady = false;
bool awake = false;
float tiltDegrees = 0.0f;
int headPosition = 90;
int targetPosition = 90;
uint32_t tiltSince = 0;
uint32_t lastMotion = 0;
uint32_t lastFrame = 0;
uint32_t lastInteraction = 0;
bool wasTouched = false;
bool wifiConnected = false;
uint32_t lastWiFiCheck = 0;
uint32_t lastGatewayCheck = 0;
String gatewayStatus = "NOT TESTED";
String voiceReply = "";
bool gatewayBusy = false;
String sessionId;

bool ensureTlsClock() {
  // TLS certificate validity checks require a real UTC clock.
  if (time(nullptr) > 1700000000) return true;
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  const uint32_t start = millis();
  while (millis() - start < 12000) {
    if (time(nullptr) > 1700000000) return true;
    delay(200);
  }
  Serial.println("TLS: NTP synchronization failed");
  return false;
}

void maintainWiFi() {
  if (millis() - lastWiFiCheck < 10000 && lastWiFiCheck) return;
  lastWiFiCheck = millis();
  wl_status_t status = WiFi.status();
  wifiConnected = status == WL_CONNECTED;
  if (wifiConnected) return;
  static uint32_t lastAttempt = 0;
  if (!strlen(WIFI_SSID)) {
    gatewayStatus = "WIFI SSID MISSING";
    return;
  }
  if (!strlen(WIFI_PASSWORD)) {
    gatewayStatus = "WIFI PASSWORD MISSING";
    Serial.println("WiFi: password missing; set STACKCHAN_WIFI_PASSWORD in GitHub Actions secrets");
    return;
  }
  if (lastAttempt && millis() - lastAttempt < 15000) return;
  lastAttempt = millis();
  Serial.printf("WiFi reconnect: SSID=%s status=%d\\n", WIFI_SSID, (int)status);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Voice Stream currently accepts TEXT only (chatInput/sessionId).
// This test sends a small message; it does not upload microphone audio.
// Production endpoint is HTTPS, so TLS CA validation must be configured
// before enabling live requests.
void drawFace();
void testGatewayText() {
  if (gatewayBusy || WiFi.status() != WL_CONNECTED) {
    gatewayStatus = "WIFI NOT CONNECTED";
    return;
  }
  if (!strlen(VOICE_GATEWAY_CA_CERT)) {
    gatewayStatus = "SET GATEWAY CA CERT";
    return;
  }
  if (!ensureTlsClock()) { gatewayStatus = "TLS CLOCK NOT READY"; return; }
  gatewayBusy = true;
  gatewayStatus = "CONNECTING SUMANEE...";
  drawFace();
  WiFiClientSecure client;
  client.setCACert(VOICE_GATEWAY_CA_CERT);
  HTTPClient http;
  if (!http.begin(client, SUMANEE_VOICE_URL)) {
    gatewayStatus = "INVALID VOICE URL";
  } else {
    http.setTimeout(12000);
    http.addHeader("Content-Type", "application/json");
    String payload = "{\"sessionId\":\"" + sessionId +
                     "\",\"chatInput\":\"สวัสดีสุมานี จาก StackChan\"}";
    int code = http.POST(payload);
    if (code == 200) {
      gatewayStatus = "GATEWAY HTTP 200";
      // Response may be streamed JSON/event frames rather than plain text.
      // Keep raw response out of the constrained LCD and log only brief status.
    } else {
      gatewayStatus = "GATEWAY HTTP " + String(code);
    }
    http.end();
  }
  gatewayBusy = false;
}

static constexpr int AUDIO_RATE = 16000;
static constexpr int AUDIO_CHUNK = 320;
static constexpr int AUDIO_SAMPLES = 16000;
int16_t* audioBuffer = nullptr;
bool audioBusy = false;
String audioStatus = "WELCOME - SUMANEE READY";
static const char* WELCOME_SPEECH = "สวัสดีค่ะ สุมานีค่ะ ยินดีต้อนรับนะคะ ตอนนี้สุมานีพร้อมแล้ว มีอะไรให้ช่วยก็บอกได้เลยค่ะ";
uint32_t welcomeShownAt = 0;
bool mouthSpeaking = false;
uint8_t mouthLevel = 0;
uint32_t audioStatusUntil = 0;

// Local audio loopback test. No network, speech recognition or cloud keys.
// CoreS3 mic and speaker must be used sequentially, not simultaneously.
void testAudioLoopback() {
  if (!audioBuffer || audioBusy) return;
  audioBusy = true;
  audioStatus = "RECORDING 1 SEC...";
  audioStatusUntil = millis() + 5000;
  M5.Speaker.end();
  if (!M5.Mic.begin()) {
    audioStatus = "MIC START FAILED";
    audioBusy = false;
    return;
  }
  int count = 0;
  uint32_t deadline = millis() + 3500;
  while (count < AUDIO_SAMPLES && millis() < deadline) {
    if (M5.Mic.record(audioBuffer + count, AUDIO_CHUNK, AUDIO_RATE)) {
      count += AUDIO_CHUNK;
    }
    delay(1);
  }
  while (M5.Mic.isRecording() && millis() < deadline + 1000) delay(1);
  M5.Mic.end();
  if (count < AUDIO_SAMPLES) {
    audioStatus = "MIC TIMEOUT";
  } else {
    audioStatus = "PLAYING...";
    M5.Speaker.setVolume(100);
    if (M5.Speaker.begin()) {
      M5.Speaker.playRaw(audioBuffer, AUDIO_SAMPLES, AUDIO_RATE, false, 1, 0);
      uint32_t playDeadline = millis() + 4000;
      while (M5.Speaker.isPlaying() && millis() < playDeadline) delay(5);
      audioStatus = "AUDIO TEST DONE";
    } else {
      audioStatus = "SPEAKER START FAILED";
    }
    M5.Speaker.end();
  }
  audioStatusUntil = millis() + 4000;
  audioBusy = false;
}


static void putLE16(uint8_t* p, uint16_t v) {
  p[0] = v & 255; p[1] = (v >> 8) & 255;
}
static void putLE32(uint8_t* p, uint32_t v) {
  for (int i=0; i<4; ++i) p[i] = (v >> (8*i)) & 255;
}
static uint16_t getLE16(const uint8_t* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
static uint32_t getLE32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1])<<8) |
         (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
}
void talkToSumanee() {
  if (audioBusy) return;
  if (WiFi.status() != WL_CONNECTED) { audioStatus = "WIFI OFFLINE"; return; }
  const bool useBridge = strlen(STACKCHAN_VOICE_BRIDGE_URL) > 0;
  if (useBridge && !strlen(STACKCHAN_VOICE_TOKEN)) {
    audioStatus = "BRIDGE TOKEN NOT SET"; return;
  }
  if (!strlen(VOICE_GATEWAY_CA_CERT)) {
    audioStatus = "VOICE TLS CA NOT SET"; return;
  }
  if (!ensureTlsClock()) { audioStatus = "TLS CLOCK NOT READY"; return; }
  audioBusy = true;
  const size_t samples = 32000;  // 2 seconds, mono PCM16 at 16kHz
  int16_t* pcm = (int16_t*)heap_caps_malloc(samples * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pcm) { audioStatus = "NO PSRAM FOR MIC"; audioBusy = false; return; }
  audioStatus = "LISTENING (2 SEC)";
  drawFace();
  M5.Speaker.end();
  size_t count = 0;
  if (M5.Mic.begin()) {
    uint32_t deadline = millis() + 5500;
    while (count < samples && millis() < deadline) {
      if (M5.Mic.record(pcm + count, AUDIO_CHUNK, AUDIO_RATE)) count += AUDIO_CHUNK;
      delay(1);
    }
    while (M5.Mic.isRecording() && millis() < deadline + 1000) delay(1);
    M5.Mic.end();
  }
  if (count != samples) {
    audioStatus = "MIC RECORD FAILED";
    heap_caps_free(pcm); audioBusy = false; return;
  }
  size_t wavLen = 44 + samples * 2;
  uint8_t* wav = (uint8_t*)heap_caps_malloc(wavLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!wav) { audioStatus = "NO PSRAM FOR WAV"; heap_caps_free(pcm); audioBusy = false; return; }
  memcpy(wav, "RIFF", 4); putLE32(wav+4, wavLen-8);
  memcpy(wav+8, "WAVEfmt ", 8); putLE32(wav+16, 16);
  putLE16(wav+20, 1); putLE16(wav+22, 1); putLE32(wav+24, AUDIO_RATE);
  putLE32(wav+28, AUDIO_RATE*2); putLE16(wav+32, 2); putLE16(wav+34, 16);
  memcpy(wav+36, "data", 4); putLE32(wav+40, samples*2);
  memcpy(wav+44, pcm, samples*2);
  heap_caps_free(pcm);
  audioStatus = "THINKING...";
  drawFace();
  WiFiClientSecure client;
  client.setCACert(VOICE_GATEWAY_CA_CERT);
  HTTPClient http;
  if (!http.begin(client, useBridge ? STACKCHAN_VOICE_BRIDGE_URL : SUMANEE_CORES3_WAV_URL)) {
    audioStatus = "BAD BRIDGE URL";
  } else {
    http.setTimeout(60000);
    http.addHeader("Content-Type", "audio/wav");
    if (useBridge) http.addHeader("Authorization", String("Bearer ") + STACKCHAN_VOICE_TOKEN);
    http.addHeader("X-Session-Id", sessionId);
    int code = http.POST(wav, wavLen);
    heap_caps_free(wav);
    wav = nullptr;
    // Both the n8n webhook and the optional voice bridge now return PCM WAV.
    // Never call getString() on binary audio: it corrupts the response.
    if (code == 200) {
      int len = http.getSize();
      if (len > 44 && len <= 1500000) {
        uint8_t* reply = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (reply) {
          WiFiClient* stream = http.getStreamPtr();
          int read = 0;
          uint32_t deadline = millis() + 45000;
          while (read < len && millis() < deadline) {
            int avail = stream->available();
            if (avail > 0) {
              int n = stream->readBytes(reply + read, min(avail, len-read));
              if (n > 0) read += n;
            } else delay(2);
          }
          if (read == len && memcmp(reply, "RIFF", 4)==0 &&
              memcmp(reply+8, "WAVE", 4)==0) {
            size_t pos = 12, dataPos = 0, dataLen = 0;
            uint32_t rate = 24000;
            uint16_t channels = 0, bits = 0, format = 0;
            while (pos + 8 <= (size_t)len) {
              uint32_t sz = getLE32(reply+pos+4);
              if (sz > (size_t)len-pos-8) break;
              if (memcmp(reply+pos, "fmt ", 4)==0 && sz>=16) {
                format=getLE16(reply+pos+8); channels=getLE16(reply+pos+10);
                rate=getLE32(reply+pos+12); bits=getLE16(reply+pos+22);
              }
              if (memcmp(reply+pos, "data", 4)==0) {
                dataPos=pos+8; dataLen=sz; break;
              }
              pos += 8 + sz + (sz & 1);
            }
            if (format==1 && channels==1 && bits==16 && dataLen>=2 &&
                rate>=8000 && rate<=48000) {
              audioStatus = "SPEAKING...";
              drawFace();
              M5.Speaker.setVolume(90);
              if (M5.Speaker.begin()) {
                M5.Speaker.playRaw((const int16_t*)(reply+dataPos), dataLen/2, rate, false, 1, 0);
                uint32_t end = millis()+30000;
                // Animate mouth using the amplitude envelope of the WAV samples.
                mouthSpeaking = true;
                const int16_t* voiceSamples = (const int16_t*)(reply + dataPos);
                const size_t voiceCount = dataLen / 2;
                const uint32_t started = millis();
                while (M5.Speaker.isPlaying() && millis() < end) {
                  size_t sampleOffset = (size_t)((uint64_t)(millis() - started) * rate / 1000);
                  if (sampleOffset >= voiceCount) sampleOffset = voiceCount - 1;
                  size_t sampleEnd = sampleOffset + rate / 25; // 40 ms envelope
                  if (sampleEnd > voiceCount) sampleEnd = voiceCount;
                  uint32_t peak = 0;
                  for (size_t k = sampleOffset; k < sampleEnd; k += 16) {
                    int32_t v = voiceSamples[k];
                    uint32_t amplitude = (uint32_t)(v < 0 ? -v : v);
                    if (amplitude > peak) peak = amplitude;
                  }
                  mouthLevel = peak < 1100 ? 0 : (peak < 6500 ? 1 : 2);
                  drawFace();
                  delay(55);
                }
                mouthSpeaking = false;
                mouthLevel = 0;
                M5.Speaker.end();
                audioStatus = "TAP BOTTOM TO TALK";
              } else audioStatus = "SPEAKER FAILED";
            } else audioStatus = "BAD WAV FORMAT";
          } else audioStatus = "WAV DOWNLOAD FAILED";
          heap_caps_free(reply);
        } else audioStatus = "NO PSRAM FOR REPLY";
      } else audioStatus = "BAD REPLY SIZE";
    } else {
      audioStatus = "VOICE HTTP " + String(code);
      heap_caps_free(wav); wav = nullptr;
    }
    http.end();
  }
  if (wav) heap_caps_free(wav);
  audioBusy = false;
}

void drawFace() {
  auto& d = M5.Display;
  const int w = d.width();
  const int h = d.height();
  const int cx = w / 2;
  const int cy = h / 2;
  const uint32_t bg = d.color565(13, 24, 40);
  const uint32_t cyan = d.color565(83, 226, 232);
  const uint32_t white = d.color565(230, 245, 250);
  const uint32_t muted = d.color565(130, 153, 170);
  const uint32_t footer = d.color565(24, 66, 78);

  // Landscape 320x240: top header 24px, centered character, bottom status 64px.
  d.startWrite();
  d.fillScreen(bg);
  d.setTextDatum(top_center);
  d.setTextSize(1);
  d.setTextColor(muted, bg);
  d.drawString(wifiConnected ? "SUMANEE  /  WIFI ONLINE" :
               (gatewayStatus == "WIFI PASSWORD MISSING" ?
                "WIFI PASSWORD MISSING" : "SUMANEE  /  WIFI OFFLINE"), cx, 5, 2);

#if HAS_SUMANEE_PORTRAIT
  // Keep the entire 120x120 portrait visible in the landscape content area.
  const int scale = 1;
  const int imageX = (w - PORTRAIT_W * scale) / 2;
  const int imageY = 31 + (138 - PORTRAIT_H * scale) / 2;
  for (int py = 0; py < PORTRAIT_H; ++py) {
    for (int px = 0; px < PORTRAIT_W; ++px) {
      const int idx = py * PORTRAIT_W + px;
      const uint8_t packed = PORTRAIT_PIXELS[idx / 2];
      const uint8_t colorIdx = (idx & 1) ? (packed & 15) : (packed >> 4);
      d.fillRect(imageX + px * scale, imageY + py * scale,
                 scale, scale, PORTRAIT_PALETTE[colorIdx]);
    }
  }
  if (mouthSpeaking) {
    // Overlay position relative to portrait, not the full screen.
    const int mx = imageX + (PORTRAIT_W * scale) / 2;
    const int my = imageY + (PORTRAIT_H * scale * 3) / 4;
    const uint16_t dark = d.color565(10, 32, 59);
    const uint16_t lip = d.color565(109, 193, 222);
    if (mouthLevel == 0) d.drawFastHLine(mx - 9, my, 18, lip);
    else if (mouthLevel == 1) d.fillRoundRect(mx - 9, my - 2, 18, 7, 3, dark);
    else d.fillRoundRect(mx - 10, my - 4, 20, 12, 5, dark);
  }
#else
  const int eyeY = 91;
  const int gap = 47;
  if (awake) {
    d.fillRoundRect(cx-gap-24, eyeY-18, 48, 36, 16, cyan);
    d.fillRoundRect(cx+gap-24, eyeY-18, 48, 36, 16, cyan);
    if (mouthSpeaking) {
      if (mouthLevel == 0) d.drawFastHLine(cx-24, cy+23, 48, cyan);
      else if (mouthLevel == 1) d.fillRoundRect(cx-20, cy+17, 40, 13, 6, cyan);
      else d.fillRoundRect(cx-23, cy+11, 46, 27, 12, cyan);
    } else {
      d.drawArc(cx, cy+23, 37, 29, 20, 160, cyan);
    }
  } else {
    d.fillRoundRect(cx-gap-24, eyeY-4, 48, 8, 4, cyan);
    d.fillRoundRect(cx+gap-24, eyeY-4, 48, 8, 4, cyan);
    d.drawFastHLine(cx-24, cy+24, 48, cyan);
  }
#endif

  // Bottom panel: text and interaction feedback remain clear of the portrait.
  d.fillRoundRect(6, h - 65, w - 12, 61, 8, footer);
  d.setTextColor(white, footer);
  d.drawString(awake ? "AWAKE" : "IDLE", cx, h - 62, 2);
  d.setTextColor(muted, footer);
  d.drawString(servoReady ? "HEAD SERVO READY" :
               "SERVO DISABLED - CHECK WIRING", cx, h - 43, 1);
  d.setTextColor(white, footer);
  d.drawString(audioStatus, cx, h - 24, 1);
  d.endWrite();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Imu.begin();
  // Short startup chime; plays locally without Wi-Fi or cloud TTS.
  M5.Speaker.setVolume(65);
  if (M5.Speaker.begin()) {
    M5.Speaker.tone(880, 110);
    delay(145);
    M5.Speaker.tone(1175, 160);
    delay(200);
    M5.Speaker.end();  // Release audio hardware for microphone capture.
  }
  audioBuffer = (int16_t*)heap_caps_malloc(AUDIO_SAMPLES * sizeof(int16_t), MALLOC_CAP_8BIT);
  if (!audioBuffer) audioStatus = "AUDIO MEMORY FAILED";

#if STACKCHAN_SERVO_PIN >= 0
  // Servo must use a safe GPIO verified for YOUR base; never connect
  // motor power directly to a GPIO or assume a Core2 pinout on CoreS3.
  headServo.setPeriodHertz(50);
  servoReady = headServo.attach(STACKCHAN_SERVO_PIN, 500, 2500) > 0;
  if (servoReady) headServo.write(headPosition);
#endif

  sessionId = "cores3-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  WiFi.mode(WIFI_STA);
  if (strlen(WIFI_SSID) && strlen(WIFI_PASSWORD)) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastInteraction = millis();
  awake = true;
  welcomeShownAt = millis();
  drawFace();
  Serial.printf("Welcome speech: %s\\n", WELCOME_SPEECH);
  Serial.printf("CoreS3 ready; servo pin=%d, enabled=%d\n", STACKCHAN_SERVO_PIN, servoReady);
}

void loop() {
  M5.update();
  maintainWiFi();
  const uint32_t now = millis();
  if (welcomeShownAt && now - welcomeShownAt >= 6000) {
    welcomeShownAt = 0;
    audioStatus = "TAP BOTTOM TO TALK";
  }
  float ax = 0, ay = 0, az = 0;
  if (M5.Imu.getAccel(&ax, &ay, &az)) {
    tiltDegrees = atan2f(ax, sqrtf(ay*ay + az*az)) * 180.0f / PI;
    if (fabsf(tiltDegrees) > 25.0f) {
      if (tiltSince == 0) tiltSince = now;
      if (!awake && now - tiltSince > 400) {
        awake = true;
        lastInteraction = now;
        targetPosition = (tiltDegrees > 0) ? STACKCHAN_SERVO_MAX_DEG : STACKCHAN_SERVO_MIN_DEG;
      }
    } else {
      tiltSince = 0;
    }
  }

  // Touch screen wakes the face; touch again cycles head direction.
  const bool touched = M5.Touch.getCount() > 0;
  if (touched && !wasTouched && M5.Touch.getDetail(0).y > M5.Display.height() - 42) {
    talkToSumanee();
    awake = true;
    lastInteraction = millis();
  } else if (touched && !wasTouched) {
    awake = true;
    lastInteraction = now;
    targetPosition = (targetPosition == STACKCHAN_SERVO_MAX_DEG)
      ? STACKCHAN_SERVO_MIN_DEG : STACKCHAN_SERVO_MAX_DEG;
  }
  wasTouched = touched;

  if (awake && now - lastInteraction > 5000) {
    awake = false;
    targetPosition = 90;
  }

  // Slow one-degree steps, non-blocking. Only runs with verified servo GPIO.
  if (servoReady && now - lastMotion >= 25) {
    lastMotion = now;
    if (headPosition < targetPosition) ++headPosition;
    else if (headPosition > targetPosition) --headPosition;
    if (headPosition != targetPosition) headServo.write(headPosition);
    else headServo.write(targetPosition);
  }

  // Avoid clearing and repainting the full LCD every 150 ms.
  // Refresh only when the face or visible status changes.
  static bool previousAwake = false;
  static bool previousWifi = false;
  static String previousAudioStatus;
  static String previousGatewayStatus;
  if (awake != previousAwake ||
      wifiConnected != previousWifi ||
      audioStatus != previousAudioStatus ||
      gatewayStatus != previousGatewayStatus) {
    previousAwake = awake;
    previousWifi = wifiConnected;
    previousAudioStatus = audioStatus;
    previousGatewayStatus = gatewayStatus;
    drawFace();
  }
  delay(5);
}
