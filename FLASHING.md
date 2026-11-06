# Flash M5Stack CoreS3 via ESPConnect

## Recommended: download a GitHub Actions build

1. Open **Actions** in this repository, select **Build CoreS3 Firmware**, and select **Run workflow** (or use the latest successful run).
2. Wait for the green checkmark. Download the artifact **CoreS3-ESPConnect-firmware**.
3. Unzip it to obtain **firmware-merged.bin**.
4. Connect the M5Stack CoreS3 to your computer with a data-capable USB-C cable.
5. Open https://thelastoutpostworkshop.github.io/ESPConnect/ in Chrome/Edge.
6. Connect to the board's serial port. Select **firmware-merged.bin** and use flash offset **0x0**.
7. Flash, wait for completion, and reset the board.

**Important:** Do not flash a standalone `firmware.bin` at offset 0x0. This project produces a *merged* image with ESP32-S3 bootloader, partitions, boot_app0 and application. Back up existing firmware/data before erasing flash.

## Troubleshooting

- `invalid header: 0xffffffff`: verify the image, address `0x0`, correct ESP32-S3 target, and that flashing completed. Erase flash only after backing up data, then retry.
- `Couldn't sync to ESP` / `setSignals`: close other serial monitors, reconnect USB, try a data cable or manual bootloader mode.
- To build in Codespaces: `git pull origin main && bash build.sh`.
- To validate a local merged image: `python -m esptool --chip esp32s3 image_info firmware-merged.bin`.

**Servo remains disabled** by default until the exact StackChan base, GPIO and power wiring are verified.
