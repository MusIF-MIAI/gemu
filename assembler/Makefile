# gasm — GE-120 / GE-130 assembler
#
# Links the shared unified-format writer (../binimage.c) so gasm, gdis and gemu
# all agree on the binary header byte-for-byte.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -I..

gasm: gasm.c ../binimage.c
	$(CC) $(CFLAGS) -o $@ gasm.c ../binimage.c

# Assemble the bundled examples into ./build/*.bin
EXAMPLES := $(wildcard examples/*.s)
examples: gasm
	@mkdir -p build
	@for s in $(EXAMPLES); do \
		b=build/$$(basename $$s .s).bin; \
		./gasm -o $$b $$s && \
		printf '  %-20s -> %s\n' "$$s" "$$b"; \
	done

clean:
	rm -f gasm a.bin
	rm -rf build

.PHONY: examples clean
