# Host build of the FIRMWARE PAC decoder, run against committed captures without a device.
SRC := ../../../firmware/application/src/rfid/nfctag/lf
INC := -Istubs -I$(SRC) -I$(SRC)/.. -I$(SRC)/../..
CFLAGS := -std=c11 -O2 -Wall -Wextra -I.

pacdemod: main.c $(SRC)/protocols/pac.c
	$(CC) $(CFLAGS) $(INC) -o $@ main.c $(SRC)/protocols/pac.c

.PHONY: clean
clean:
	rm -f pacdemod
