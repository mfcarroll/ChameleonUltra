CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined

test: test_lf_ia_codec
	./test_lf_ia_codec

test_lf_ia_codec: test_lf_ia_codec.c ../lf_ia_codec.c ../lf_ia_codec.h
	$(CC) $(CFLAGS) -o $@ test_lf_ia_codec.c ../lf_ia_codec.c

clean:
	rm -f test_lf_ia_codec

.PHONY: test clean
