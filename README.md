# Smart Hotel StackChan

Prototype for **M5Stack Core2 only** (verify physical hardware before flashing). Check-in and light controls are simulated; housekeeping POST needs a running API. StackChan servos are not yet configured.

## Cloud workflow

1. Open this repository in GitHub Codespaces (Code → Codespaces → Create codespace).
2. Run `bash build.sh`. The container installs PlatformIO and esptool on creation.
3. If build succeeds, download `firmware-merged.bin` from Codespaces.
4. Connect M5Stack Core2 with a data USB cable to Chrome/Edge and open https://thelastoutpostworkshop.github.io/ESPConnect/.
5. Flash the merged binary at **0x0 only for ESP32 Core2**. Back up any existing firmware/data first.

Never commit Wi-Fi credentials or API secrets. No firmware has been built or tested on hardware as part of this repository setup. For real hotel check-in, add backend authentication, guest authorization and audit logs. Never connect mains electricity directly to ESP32 GPIO.
