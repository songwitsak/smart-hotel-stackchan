# Smart Hotel StackChan — M5Stack CoreS3

Phase 1: expressive face on CoreS3 LCD, tilt-to-wake using IMU, touch interaction, and **optional** smooth servo head movement. No Wi-Fi, voice, payment or hotel APIs in this phase.

## Hardware safety / servo checklist
- Identify your exact StackChan base/servo driver and its **CoreS3 GPIO signal pin**. Core2 pin mappings are not interchangeable.
- Confirm whether your base uses **direct PWM servo**, an I2C PWM controller, or a different motor interface. This firmware supports **direct PWM only**.
- Check servo voltage, current draw, ground reference, mechanical limits and safe neutral position. Do **not** power a servo directly from an ESP32 GPIO.
- By default `STACKCHAN_SERVO_PIN=-1`: the face and IMU work but the servo stays **disabled** until a verified pin is configured.
- Once confirmed, edit `platformio.ini` build flags with the correct GPIO and safe min/max angles. Start with the horn disconnected or unobstructed; ensure no binding.

## Cloud build
Run `git pull origin main && bash build.sh` in GitHub Codespaces.
If successful, download `firmware-merged.bin` and flash **at 0x0** using a CoreS3-compatible ESP32-S3 flasher. Verify your board variant and back up existing firmware first.

## Interaction
- Idle: closed eyes
- Tilt device over 25 degrees for 400 ms: wake and change target head position
- Tap touchscreen: wake and alternate head target
- After 5 seconds: sleep and center head

**Status:** Source updated, not yet cloud-compiled or hardware-tested. Servo movement requires wiring verification and an enabled PWM pin.

## Phase 2: Local microphone / speaker test (new)

Tap the **bottom status bar** on the CoreS3 display. The device records approximately one second of microphone audio and plays it through its built-in speaker. The screen shows RECORDING / PLAYING / DONE (or an error). Other touch areas still wake the face; IMU and optional servo remain intact.

This is a **local loopback test only**: no Wi-Fi, STT, TTS, WebSocket, Sumanee, hotel booking, light control or payment yet. The mic and speaker run sequentially to avoid simultaneous audio conflicts.

Rebuild with GitHub Actions or `bash build.sh`, then flash the updated merged image. Build and physical audio functionality are not yet verified.
