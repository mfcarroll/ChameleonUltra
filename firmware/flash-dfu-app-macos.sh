#!/bin/bash
# Build + flash the application over USB DFU on macOS. Fully automated.
#
#   ./flash-dfu-app-macos.sh [--no-build]
#
# BUILD + SIGN runs in the project's own Docker image (docker-compose.yml), which
# carries Nordic's nrfutil and the pinned ARM toolchain. FLASH uses nrfutil on the
# host, because Docker Desktop on macOS cannot pass USB through to a container.
#
# ⚠ INSTALL nrfutil THE WAY THE WIKI SAYS, NOT VIA HOMEBREW. The Homebrew cask was
# disabled 2026-09-01 for failing the Gatekeeper check, which is easy to read as
# "no nrfutil on macOS" — it is not. Nordic ships a signed native arm64 build, and
# curl does not set com.apple.quarantine, so there is no Gatekeeper prompt at all:
#
#   curl -sL -o ~/bin/nrfutil \
#     https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/aarch64-apple-darwin/nrfutil
#   chmod 755 ~/bin/nrfutil
#   nrfutil install device nrf5sdk-tools
#
# (The old developer.nordicsemi.com/.pc-tools/... paths now 410 for macOS; the
# executables moved to files.nordicsemi.com under Rust target triples.)
#
# ⛔ Do NOT use the PyPI `nrfutil`. It ends at 6.1.7 (2022) because Nordic moved the
# tool to Rust at v7; 6.1.7 hard-requires pc_ble_driver_py, which has no arm64
# wheel, and its `dfu usb-serial` never completes the handshake with this
# bootloader. Measured 2026-09-11, after an afternoon of trying.
#
# The Linux scripts flash-dfu-app.sh / flash-dfu-full.sh need lsusb, hence this one.
#
# App-only: bootloader and SoftDevice are untouched, so a failed flash leaves the
# device in DFU mode and recoverable by re-running.
set -euo pipefail
cd -- "$(dirname "$0")"

NRFUTIL="${NRFUTIL:-$(command -v nrfutil || echo "$(cd .. && pwd)/../.tools/bin/nrfutil")}"
PY="${PY:-$(cd .. && pwd)/software/script/.venv/bin/python}"
PKG=objects/ultra-dfu-app.zip

if [[ "${1:-}" != "--no-build" ]]; then
  docker info >/dev/null 2>&1 || { echo "Docker daemon not running: open -a Docker"; exit 1; }
  docker compose up --pull=always build-ultra
fi
[[ -f $PKG ]] || { echo "No $PKG — build first (drop --no-build)."; exit 1; }
[[ -x $NRFUTIL ]] || { echo "nrfutil not found — see the install note at the top of this script."; exit 1; }

echo "==> Package: $PKG ($(stat -f%z $PKG) bytes)"
echo "==> Entering DFU mode"
"$PY" ../resource/tools/enter_dfu.py || {
  echo "   Trigger it by hand: unplug, hold B, plug in (LEDs 4 & 5 blink), then --no-build."; exit 1; }

# ⛔ POLL, DO NOT SLEEP. This was `sleep 3` and it failed: nrfutil reported "No devices with
# requested serial number(s) or trait(s) found" while the device was, moments later, sitting
# in DFU perfectly happily. The bootloader's USB enumeration and its inactivity window do not
# line up with any fixed delay — 2s was enough on the retry that worked. Waiting for the
# TRAIT is both faster and correct. (The failure is harmless: app-only DFU leaves the
# bootloader intact, so a missed flash just means running this again.)
echo "==> Waiting for the DFU bootloader to enumerate"
for i in $(seq 1 30); do
  "$NRFUTIL" device list 2>/dev/null | grep -q nordicDfu && { echo "    up after ${i}s"; break; }
  sleep 1
done
"$NRFUTIL" device list 2>/dev/null | grep -q nordicDfu || {
  echo "   Bootloader never appeared. Unplug, hold B, plug in (LEDs 4 & 5 blink), then --no-build."
  exit 1; }

echo "==> Flashing"
"$NRFUTIL" device program --firmware "$PKG" --traits nordicDfu
echo "==> Done. Verify with:"
echo "    cd ../software/script && .venv/bin/python cu.py \"hw version\""
