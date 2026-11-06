#!/usr/bin/env bash
set -euo pipefail

ENV_NAME="m5stack-core2"
BUILD_DIR=".pio/build/$ENV_NAME"

python -m platformio run -e "$ENV_NAME"

BOOT_APP="$BUILD_DIR/boot_app0.bin"
if [[ ! -f "$BOOT_APP" ]]; then
  BOOT_APP="$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages" -type f -name boot_app0.bin -print -quit 2>/dev/null || true)"
fi
if [[ -z "$BOOT_APP" || ! -f "$BOOT_APP" ]]; then
  echo "ERROR: boot_app0.bin not found in PlatformIO packages." >&2
  exit 1
fi

for file in "$BUILD_DIR/bootloader.bin" "$BUILD_DIR/partitions.bin" "$BUILD_DIR/firmware.bin"; do
  if [[ ! -f "$file" ]]; then
    echo "ERROR: Missing $file" >&2
    exit 1
  fi
done

python -m esptool --chip esp32 merge_bin \
  -o firmware-merged.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$BUILD_DIR/bootloader.bin" \
  0x8000 "$BUILD_DIR/partitions.bin" \
  0xe000 "$BOOT_APP" \
  0x10000 "$BUILD_DIR/firmware.bin"

echo "Created firmware-merged.bin (flash at address 0x0 on M5Stack Core2)"
