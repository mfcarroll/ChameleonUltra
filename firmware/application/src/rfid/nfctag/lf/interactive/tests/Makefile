CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined

test: test_lf_ia_codec test_lf_ia_hitag2 test_lf_hitag2_frame test_lf_ia_carrier test_lf_rdr_frame
	./test_lf_ia_codec
	./test_lf_ia_hitag2
	./test_lf_hitag2_frame
	./test_lf_ia_carrier
	./test_lf_rdr_frame

test_lf_ia_codec: test_lf_ia_codec.c ht2_answer_capture.h ht2_cu_answer_capture.h ht2_tag_answer_capture.h ../lf_ia_codec.c ../lf_ia_codec.h
	$(CC) $(CFLAGS) -o $@ test_lf_ia_codec.c ../lf_ia_codec.c

test_lf_ia_hitag2: test_lf_ia_hitag2.c ../lf_ia_hitag2.c ../lf_ia_hitag2.h ../lf_ia_codec.c ../lf_ia_codec.h ../lf_ia.h
	$(CC) $(CFLAGS) -o $@ test_lf_ia_hitag2.c ../lf_ia_hitag2.c ../lf_ia_codec.c

FRAME = ../../../../reader/lf/lf_hitag2_frame

test_lf_hitag2_frame: test_lf_hitag2_frame.c ht2_answer_capture.h ht2_no_tag_capture.h $(FRAME).c $(FRAME).h ../lf_ia_codec.c ../lf_ia_codec.h
	$(CC) $(CFLAGS) -I.. -o $@ test_lf_hitag2_frame.c $(FRAME).c ../lf_ia_codec.c

test_lf_ia_carrier: test_lf_ia_carrier.c ../lf_ia_carrier.c ../lf_ia_carrier.h
	$(CC) $(CFLAGS) -o $@ test_lf_ia_carrier.c ../lf_ia_carrier.c -lm

RDR_FRAME = ../../../../reader/lf/lf_rdr_frame

RDR_TRACE = ../../../../reader/lf/lf_rdr_trace

test_lf_rdr_frame: test_lf_rdr_frame.c $(RDR_FRAME).c $(RDR_FRAME).h $(RDR_TRACE).c $(RDR_TRACE).h ../../../../reader/lf/lf_rdr_ia.h ../lf_ia_codec.h
	$(CC) $(CFLAGS) -I.. -I../../../../reader/lf -o $@ test_lf_rdr_frame.c $(RDR_FRAME).c $(RDR_TRACE).c

clean:
	rm -f test_lf_ia_codec test_lf_ia_hitag2 test_lf_hitag2_frame test_lf_ia_carrier test_lf_rdr_frame

.PHONY: test clean
