#include <M5Unified.h>
#include <ESP32Servo.h>
#include <math.h>
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
static constexpr int AUDIO_RATE = 16000;
static constexpr int AUDIO_CHUNK = 320;
static constexpr int AUDIO_SAMPLES = 16000;
int16_t* audioBuffer = nullptr;
bool audioBusy = false;
String audioStatus = "TAP BOTTOM TO TEST MIC";
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

  d.fillScreen(bg);
  d.setTextDatum(top_center);
  d.setTextColor(muted, bg);
  d.setTextSize(1);
  d.drawString("SUMANEE  /  STACKCHAN", cx, 10, 2);

  const int eyeY = cy - 20;
  const int gap = w / 6;
  if (awake) {
    d.fillRoundRect(cx-gap-24, eyeY-18, 48, 36, 16, cyan);
    d.fillRoundRect(cx+gap-24, eyeY-18, 48, 36, 16, cyan);
    d.drawArc(cx, cy+34, 37, 29, 20, 160, cyan, bg);
  } else {
    d.fillRoundRect(cx-gap-24, eyeY-4, 48, 8, 4, cyan);
    d.fillRoundRect(cx+gap-24, eyeY-4, 48, 8, 4, cyan);
    d.drawFastHLine(cx-24, cy+35, 48, cyan);
  }

  d.setTextColor(white, bg);
  d.drawString(awake ? "AWAKE" : "IDLE", cx, h-47, 2);
  d.setTextColor(muted, bg);
  d.drawString(servoReady ? "HEAD SERVO READY" : "SERVO DISABLED - CHECK WIRING", cx, h-37, 1);
  d.fillRoundRect(8, h-25, w-16, 23, 6, d.color565(24, 66, 78));
  d.setTextColor(white, d.color565(24, 66, 78));
  d.drawString(audioStatus, cx, h-20, 1);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Imu.begin();
  audioBuffer = (int16_t*)heap_caps_malloc(AUDIO_SAMPLES * sizeof(int16_t), MALLOC_CAP_8BIT);
  if (!audioBuffer) audioStatus = "AUDIO MEMORY FAILED";

#if STACKCHAN_SERVO_PIN >= 0
  // Servo must use a safe GPIO verified for YOUR base; never connect
  // motor power directly to a GPIO or assume a Core2 pinout on CoreS3.
  headServo.setPeriodHertz(50);
  servoReady = headServo.attach(STACKCHAN_SERVO_PIN, 500, 2500) > 0;
  if (servoReady) headServo.write(headPosition);
#endif

  lastInteraction = millis();
  drawFace();
  Serial.printf("CoreS3 ready; servo pin=%d, enabled=%d\n", STACKCHAN_SERVO_PIN, servoReady);
}

void loop() {
  M5.update();
  const uint32_t now = millis();
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
    testAudioLoopback();
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

  if (now - lastFrame >= 150) {
    lastFrame = now;
    drawFace();
  }
  delay(5);
}
