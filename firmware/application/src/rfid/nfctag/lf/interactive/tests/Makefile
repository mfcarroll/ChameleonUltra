CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined

test: test_lf_ia_codec test_lf_ia_hitag2
	./test_lf_ia_codec
	./test_lf_ia_hitag2

test_lf_ia_codec: test_lf_ia_codec.c ../lf_ia_codec.c ../lf_ia_codec.h
	$(CC) $(CFLAGS) -o $@ test_lf_ia_codec.c ../lf_ia_codec.c

test_lf_ia_hitag2: test_lf_ia_hitag2.c ../lf_ia_hitag2.c ../lf_ia_hitag2.h ../lf_ia_codec.c ../lf_ia_codec.h ../lf_ia.h
	$(CC) $(CFLAGS) -o $@ test_lf_ia_hitag2.c ../lf_ia_hitag2.c ../lf_ia_codec.c

clean:
	rm -f test_lf_ia_codec test_lf_ia_hitag2

.PHONY: test clean
