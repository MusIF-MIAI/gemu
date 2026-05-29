CFLAGS = -std=c99 -Wall -Wextra -O2
GASM   = ../assembler/gasm

all: gec runrv

gec: gec.c
	$(CC) $(CFLAGS) -o $@ $<

# runrv links the emulator core; libge.a is built by the parent Makefile.
runrv: runrv.c ../libge.a ../binimage.c
	$(CC) $(CFLAGS) -I.. -o $@ runrv.c ../binimage.c ../libge.a

../libge.a:
	$(MAKE) -C .. libge.a

clean:
	rm -f gec runrv *.s *.bin

.PHONY: all clean
