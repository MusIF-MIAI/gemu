OBJS=msl.o ge.o pulse.o msl-timings.o console.o console_socket.o peripherical.o log.o reader.o cap.o transcode.o cardreader.o binimage.o alu_cc.o alu_bin.o alu_logic.o alu_dec.o alu_reg.o
CFLAGS+=-MD -MP
CC=gcc
TESTS=$(patsubst %.c,%.o,$(wildcard tests/*.c))

ge: libge.a main.o
	$(CC) $(CFLAGS) $(LDFLAGS) libge.a -o ge main.o $(OBJS)

libge.a: $(OBJS)
	$(AR) rcs libge.a $(OBJS)

tests/tests: $(TESTS) libge.a
	$(CC) $(CFLAGS) $(LDFLAGS) libge.a $^ -o $@

-include $(OBJS:%.o=%.d)
-include $(TESTS:%.o=%.d)

# Build the assembler (gasm) and disassembler (gdis) toolchain.
.PHONY: tools
tools:
	$(MAKE) -C assembler gasm
	$(MAKE) -C disassembler gdis

.PHONY: check
check: tests/tests ge tools
	tests/tests
	python3 tests/isa_consistency.py
	sh tests/roundtrip.sh

.PHONY: clean
clean:
	rm -f libge.a main.o ge tests/tests
	rm -f $(OBJS) $(OBJS:%.o=%.d)
	rm -f $(TESTS) $(TESTS:%.o=%.d)
	$(MAKE) -C assembler clean
	$(MAKE) -C disassembler clean

.PHONY: docs
docs:
	cd docs ; doxygen

.PHONY: open-docs
open-docs: docs
	open docs/html/index.html

