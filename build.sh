#!/usr/bin/env bash
set -euo pipefail
python -m platformio run -e m5stack-core2
python -m esptool --chip esp32 merge_bin -o firmware-merged.bin --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 .pio/build/m5stack-core2/bootloader.bin 0x8000 .pio/build/m5stack-core2/partitions.bin 0xe000 .pio/build/m5stack-core2/boot_app0.bin 0x10000 .pio/build/m5stack-core2/firmware.bin
echo "Created firmware-merged.bin (flash at 0x0 for ESP32 Core2)"
