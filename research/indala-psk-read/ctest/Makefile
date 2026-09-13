# Host build of the FIRMWARE decoder, so it can be run against the committed captures
# without a device. The .c under test is the one that ships — not a copy.
FW  := ../../../firmware/application/src/rfid/reader/lf
# ⭐ The EMULATION encoder, host-compiled through shim/nrf_pwm.h — the same .c that ships.
EMU := ../../../firmware/application/src/rfid/nfctag/lf/utils
PY  := ../../../software/script/.venv/bin/python
CAP := ../caps/phasebits

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wconversion -I$(FW)

# ⛔ THE HEADERS ARE DEPENDENCIES TOO, and leaving them out is the SAME false-pass bug the
# `check` target's clean-first rule was added for. Every capture length, frame size and
# preamble in this decoder family lives in a .h; editing one and re-running `make` left the
# old binary in place and printed a PASS for code that no longer existed. Caught 2026-09-13
# while chasing a GProxII mismatch that turned out NOT to be this — but the gap was real.
cdemod: main.c $(FW)/lf_indala_psk.c $(FW)/lf_indala_psk.h $(FW)/lf_ask_manchester.c $(FW)/lf_ask_manchester.h $(FW)/lf_fsk2a.c $(FW)/lf_fsk2a.h $(FW)/lf_slicer.c $(FW)/lf_slicer.h $(FW)/lf_ask_biphase.c $(FW)/lf_ask_biphase.h
	$(CC) $(CFLAGS) -o $@ main.c $(FW)/lf_indala_psk.c $(FW)/lf_ask_manchester.c $(FW)/lf_fsk2a.c $(FW)/lf_slicer.c $(FW)/lf_ask_biphase.c

# ⭐ THE ASK ARMS COMPILE THE SHIPPING EMITTERS TOO — gallagher.c, securakey.c, noralsy.c —
# not copies of them. That is the whole point of this harness (C156).
PROTO := ../../../firmware/application/src/rfid/nfctag/lf/protocols
TAGS  := ../../../firmware/application/src/rfid/nfctag

roundtrip: roundtrip.c $(FW)/lf_indala_psk.c $(FW)/lf_indala_psk.h $(FW)/lf_ask_manchester.c $(FW)/lf_ask_manchester.h $(FW)/lf_fsk2a.c $(FW)/lf_fsk2a.h $(FW)/lf_slicer.c $(EMU)/psk1.c $(EMU)/psk1.h \
           $(PROTO)/gallagher.c $(PROTO)/securakey.c $(PROTO)/noralsy.c $(PROTO)/fsk2a_t55xx.c
	$(CC) $(CFLAGS) -I$(EMU) -I$(PROTO) -I$(TAGS) -Ishim -o $@ roundtrip.c \
	  $(FW)/lf_indala_psk.c $(FW)/lf_ask_manchester.c $(FW)/lf_fsk2a.c $(FW)/lf_slicer.c $(EMU)/psk1.c \
	  $(PROTO)/gallagher.c $(PROTO)/securakey.c $(PROTO)/noralsy.c $(PROTO)/fsk2a_t55xx.c

# ⭐ The cross-check that matters: C integer decoder vs numpy decoder, same 320 captures.
# ⭐ The cross-check that matters: C integer decoder vs numpy decoder, same 320 captures,
# plus the emitter round trip — the arm that had no host coverage until C152 (L125).
# ⛔⛔ `check` REBUILDS FROM SCRATCH, AND THAT IS NOT PARANOIA. Editing a firmware source and
# running `make check` within the same second leaves make's timestamp comparison tied, so it
# declares the binaries up to date and the harness reports ✓ ON THE OLD BUILD. Observed
# 2026-09-13: a deliberately broken Securakey bit rate was restored, `make` skipped the
# rebuild, and the arm still printed its FAILURE — the same mechanism in the other direction
# would print a PASS for code that no longer exists. A false pass in a correctness harness is
# worse than a false failure, and costs a few seconds to remove entirely.
.PHONY: check
check: clean cdemod roundtrip
	@./roundtrip
	@echo
	@$(PY) compare.py

.PHONY: clean
clean:
	rm -f cdemod roundtrip
