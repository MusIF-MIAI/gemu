# gdis — GE-120 / GE-130 disassembler & depuncher
#
# Reuses the emulator's own .cap parser (../cap.c) and column transcoder
# (../transcode.c) so the depunch path is bit-identical to how gemu reads decks.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -I..

gdis: gdis.c ../cap.c ../transcode.c ../binimage.c
	$(CC) $(CFLAGS) -o $@ gdis.c ../cap.c ../transcode.c ../binimage.c

clean:
	rm -f gdis

.PHONY: clean
