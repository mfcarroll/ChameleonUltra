#!/bin/bash
# Build + sign the application DFU package on macOS, then hand it to the GUI to flash.
#
#   ./flash-dfu-app-macos.sh [--no-build]
#
# BUILD + SIGN is fully automated here, via the project's own Docker image
# (docker-compose.yml), which carries Nordic's real nrfutil and the pinned ARM
# toolchain. That is the sanctioned toolchain — do not hand-roll it.
#
# ⛔ FLASHING FROM THE CLI IS NOT SOLVED ON macOS. Measured 2026-09-11, so that the
# next person does not spend the afternoon rediscovering it:
#   - flash-dfu-app.sh needs `lsusb` (Linux-only) and `nrfutil device` (Nordic v7+).
#   - The Homebrew cask for v7+/v8 was DISABLED 2026-09-01: it fails the macOS
#     Gatekeeper check. Installing it anyway means bypassing Gatekeeper by hand.
#   - nrfutil 8.x has no macOS arm64 wheel on PyPI. 6.1.7 installs but caps at
#     Python <3.11 and hard-requires pc_ble_driver_py>=0.16.4, which has no arm64
#     wheel at all (PyPI stops at 0.11.4).
#   - nrfutil 5.2.0 installs on 3.11 but is not py3-clean: `c.encode('hex')` and
#     `dict.iteritems()` both raise in the `pkg generate` path.
#   - 6.1.7 on Python 3.9 with a pc_ble_driver_py shim DOES run, and `pkg generate`
#     works — but `dfu usb-serial` never gets a reply to SetPRN/GetSerialMTU from
#     this bootloader, with or without DTR asserted (the device's own enter_dfu.py
#     notes DTR is required) and at every connect-delay tried. Suspect
#     __ensure_bootloader()'s DeviceLister/DFUTrigger path, which runs BEFORE the
#     port is opened and may be knocking the device back out of DFU.
#
# ⇒ Flash the zip this script builds with ChameleonUltraGUI, which implements DFU
#   itself and is known to work with this device.
set -euo pipefail
cd -- "$(dirname "$0")"
PKG=objects/ultra-dfu-app.zip

if [[ "${1:-}" != "--no-build" ]]; then
  docker info >/dev/null 2>&1 || { echo "Docker daemon not running: open -a Docker"; exit 1; }
  docker compose up --pull=always build-ultra
fi
[[ -f $PKG ]] || { echo "No $PKG — build first (drop --no-build)."; exit 1; }

cat <<MSG

Signed package ready:
  $(pwd)/$PKG   ($(stat -f%z $PKG) bytes)

To flash: open ChameleonUltraGUI, connect the device, and choose the option to
flash firmware from a local DFU zip, pointing it at the file above.

App-only: the bootloader and SoftDevice are untouched, so a failed flash leaves
the device recoverable.
MSG
