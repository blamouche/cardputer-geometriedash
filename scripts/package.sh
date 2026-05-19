#!/usr/bin/env bash
# Build the firmware and lay out versioned artifacts in dist/.
#
#   dist/cardputer-geometriedash-v<X.Y.Z>.bin          - app image only
#                                                         (flashes to 0x10000)
#   dist/cardputer-geometriedash-v<X.Y.Z>-merged.bin   - single image with
#                                                         bootloader + partition
#                                                         table + boot_app0 + app
#                                                         (flashes to 0x0)
#
# The merged image is what most users want: one esptool command to a fresh
# Cardputer and the game runs.

set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(tr -d '[:space:]' < VERSION)
BUILD=.pio/build/cardputer
DIST=dist

echo "== Building Geometry Dash v$VERSION"
pio run -e cardputer

mkdir -p "$DIST"
APP="$DIST/cardputer-geometriedash-v$VERSION.bin"
MERGED="$DIST/cardputer-geometriedash-v$VERSION-merged.bin"

cp "$BUILD/firmware.bin" "$APP"

BOOT_APP0=$(find "$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions" \
                -name boot_app0.bin 2>/dev/null | head -1 || true)

if [[ -z "${BOOT_APP0:-}" ]]; then
    echo "warn: boot_app0.bin not found - merged image will be skipped"
else
    pio pkg exec -- esptool.py --chip esp32s3 merge_bin \
        -o "$MERGED" \
        --flash_mode dio --flash_freq 80m --flash_size 8MB \
        0x0000  "$BUILD/bootloader.bin" \
        0x8000  "$BUILD/partitions.bin" \
        0xE000  "$BOOT_APP0" \
        0x10000 "$BUILD/firmware.bin"
fi

echo
echo "== Artifacts in $DIST/"
ls -lh "$DIST"
