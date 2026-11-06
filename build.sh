#!/usr/bin/env bash
set -euo pipefail

ENV_NAME="m5stack-cores3"
BUILD_DIR=".pio/build/$ENV_NAME"
python -m platformio run -e "$ENV_NAME"

BOOT_APP="$BUILD_DIR/boot_app0.bin"
if [[ ! -f "$BOOT_APP" ]]; then
  BOOT_APP="$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages" -type f -name boot_app0.bin -print -quit 2>/dev/null || true)"
fi
if [[ -z "$BOOT_APP" || ! -f "$BOOT_APP" ]]; then
  echo "ERROR: boot_app0.bin not found" >&2
  exit 1
fi
for file in bootloader.bin partitions.bin firmware.bin; do
  test -f "$BUILD_DIR/$file" || { echo "Missing $BUILD_DIR/$file" >&2; exit 1; }
done

# ESP32-S3 bootloader starts at 0x0 (unlike classic ESP32 at 0x1000).
python -m esptool --chip esp32s3 merge_bin \
  -o firmware-merged.bin \
  --flash_mode dio --flash_freq 40m --flash_size 16MB \
  0x0 "$BUILD_DIR/bootloader.bin" \
  0x8000 "$BUILD_DIR/partitions.bin" \
  0xe000 "$BOOT_APP" \
  0x10000 "$BUILD_DIR/firmware.bin"
echo "Created firmware-merged.bin for M5Stack CoreS3; flash at 0x0"
