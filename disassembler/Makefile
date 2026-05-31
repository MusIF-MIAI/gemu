# gdis — GE-120 / GE-130 disassembler & depuncher
#
# Reuses the emulator's own .cap parser (../cap.c) and column transcoder
# (../transcode.c) so the depunch path is bit-identical to how gemu reads decks.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -I..

gdis: gdis.c ../cap.c ../transcode.c ../binimage.c ../gecode.c
	$(CC) $(CFLAGS) -o $@ gdis.c ../cap.c ../transcode.c ../binimage.c ../gecode.c

# Regenerate the named (annotated) disassembly of the CPU functional-test deck
# from its checked-in .sym symbol file. funktionalcpu.sym.s is committed so the
# annotated listing is browsable without a build; `make named` refreshes it.
funktionalcpu.sym.s: gdis funktionalcpu.sym ../../DUMP1/funktionalcpu.cap
	./gdis --symbols funktionalcpu.sym -o $@ ../../DUMP1/funktionalcpu.cap

.PHONY: named
named: funktionalcpu.sym.s

clean:
	rm -f gdis

.PHONY: clean
