# gasm — GE-120 / GE-130 assembler
#
# Self-contained: gasm's output is a .cap card deck, which is the only thing a
# GE-120 can be handed a program on, so it shares no format code with gemu.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -I..

gasm: gasm.c
	$(CC) $(CFLAGS) -o $@ gasm.c

# Assemble the bundled examples into ./build/*.cap card decks
EXAMPLES := $(wildcard examples/*.s)
examples: gasm
	@mkdir -p build
	@for s in $(EXAMPLES); do \
		n=$$(basename $$s .s); \
		b=build/$$n.cap; \
		case $$n in halt|bootcard) m=--card ;; *) m= ;; esac; \
		./gasm $$m -o $$b $$s >/dev/null && \
		printf '  %-20s -> %s\n' "$$s" "$$b"; \
	done

clean:
	rm -f gasm a.cap
	rm -rf build

.PHONY: examples clean
