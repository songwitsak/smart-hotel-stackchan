#include <M5Unified.h>
#include <ESP32Servo.h>
#include <math.h>

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
  d.drawString(servoReady ? "HEAD SERVO READY" : "SERVO DISABLED - CHECK WIRING", cx, h-26, 1);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Imu.begin();

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
  if (touched && !wasTouched) {
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
