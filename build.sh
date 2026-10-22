#!/usr/bin/env bash
#
# build.sh - Build the Altair2040 RP2040 sketch via arduino-cli.
#
# Usage:
#   ./build.sh           # build for Raspberry Pi Pico (rpipico), copy .uf2 to ./build/
#   ./build.sh clean     # remove the build artifacts
#
# Requirements:
#   * arduino-cli on PATH
#   * rp2040:rp2040 core installed (earlephilhower), e.g.:
#       arduino-cli core install rp2040:rp2040
#
set -euo pipefail

SKETCH_DIR="$(pwd)"
SKETCH="${SKETCH_DIR}/altair-8800-pico.ino"
BUILD_PATH="${SKETCH_DIR}/build"
FQBN="rp2040:rp2040:rpipico"

# RP2040 core marks Bxxxx binary literals as deprecated errors (-Werror);
# this silences them without changing the original logic.
EXTRA_FLAGS="-Wno-error=deprecated-declarations -Wno-deprecated-declarations"

if [[ "${1:-}" == "clean" ]]; then
  rm -rf "${BUILD_PATH}"
  echo "Cleaned."
  exit 0
fi

echo ">> Compiling ${SKETCH} for ${FQBN} ..."
arduino-cli compile \
  --fqbn "${FQBN}" \
  --build-path "${BUILD_PATH}" \
  --build-property "build.extra_flags=${EXTRA_FLAGS}" \
  "${SKETCH_DIR}"

echo ">> Done. UF2 at: ${BUILD_PATH}/altair-8800-pico.ino.uf2"
echo ">> Flash: hold BOOTSEL on the Pico, plug in USB, copy the .uf2 to the RPI-RP2 drive."
