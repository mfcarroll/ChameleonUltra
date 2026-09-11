#!/bin/zsh
# Guided three-capture run: empty field, tag under test, known-good control.
# Captures land in ./caps/ as raw 8-bit ADC samples (125kHz, 8us/sample).
#
#   ./grab.sh
#
# A baseline is mandatory: every capture contains USB-transfer glitches, and
# without an empty-field reference you will mistake them for tag signal.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
CAPS="$HERE/caps"; mkdir -p "$CAPS"
CLI="$HERE/../../software/script"
cd "$CLI"

grab () {
  echo ""
  echo "=============================================================="
  echo "  $2"
  echo "=============================================================="
  read "?  Press RETURN when ready... "
  .venv/bin/python cu.py "hw mode -r" \
      "lf sniff --timeout 1000 --out $CAPS/$1.bin" 2>&1 \
      | grep -Ev "connected:|^$"
}

grab baseline "CAPTURE 1 of 3 - BASELINE.  Remove ALL tags. Nothing near the device."
grab indala   "CAPTURE 2 of 3 - INDALA.    Hold the indala26 tag on the LF antenna."
grab control  "CAPTURE 3 of 3 - CONTROL.   Hold a tag that DOES read (e.g. HID Prox)."

echo ""
echo "Captured. Now analyse:"
echo "  $CLI/.venv/bin/python $HERE/analyse.py $CAPS/baseline.bin $CAPS/indala.bin $CAPS/control.bin"
