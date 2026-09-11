# Host build of the FIRMWARE decoder, so it can be run against the committed captures
# without a device. The .c under test is the one that ships — not a copy.
FW  := ../../../firmware/application/src/rfid/reader/lf
PY  := ../../../software/script/.venv/bin/python
CAP := ../caps/phasebits

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wconversion -I$(FW)

cdemod: main.c $(FW)/lf_indala_psk.c $(FW)/lf_indala_psk.h
	$(CC) $(CFLAGS) -o $@ main.c $(FW)/lf_indala_psk.c

# ⭐ The cross-check that matters: C integer decoder vs numpy decoder, same 320 captures.
.PHONY: check
check: cdemod
	@$(PY) compare.py

.PHONY: clean
clean:
	rm -f cdemod
