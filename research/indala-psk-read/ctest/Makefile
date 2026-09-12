# Host build of the FIRMWARE decoder, so it can be run against the committed captures
# without a device. The .c under test is the one that ships — not a copy.
FW  := ../../../firmware/application/src/rfid/reader/lf
# ⭐ The EMULATION encoder, host-compiled through shim/nrf_pwm.h — the same .c that ships.
EMU := ../../../firmware/application/src/rfid/nfctag/lf/utils
PY  := ../../../software/script/.venv/bin/python
CAP := ../caps/phasebits

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wconversion -I$(FW)

cdemod: main.c $(FW)/lf_indala_psk.c $(FW)/lf_indala_psk.h
	$(CC) $(CFLAGS) -o $@ main.c $(FW)/lf_indala_psk.c

roundtrip: roundtrip.c $(FW)/lf_indala_psk.c $(EMU)/psk1.c $(EMU)/psk1.h
	$(CC) $(CFLAGS) -I$(EMU) -Ishim -o $@ roundtrip.c $(FW)/lf_indala_psk.c $(EMU)/psk1.c

# ⭐ The cross-check that matters: C integer decoder vs numpy decoder, same 320 captures.
# ⭐ The cross-check that matters: C integer decoder vs numpy decoder, same 320 captures,
# plus the emitter round trip — the arm that had no host coverage until C152 (L125).
.PHONY: check
check: cdemod roundtrip
	@./roundtrip
	@echo
	@$(PY) compare.py

.PHONY: clean
clean:
	rm -f cdemod roundtrip
