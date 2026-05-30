OBJS=msl.o ge.o pulse.o msl-timings.o console.o console_socket.o peripherical.o log.o reader.o cap.o transcode.o cardreader.o binimage.o disasm.o alu_cc.o alu_bin.o alu_logic.o alu_dec.o alu_reg.o
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
	sh cc/test.sh

# In-browser WebAssembly console (console/wasm/): the whole emulator compiled
# with emscripten, served as console.html + main.mjs + main.wasm.
#   make wasm      -- build it. Uses local 'emcc' if installed, otherwise runs
#                     the emscripten/emsdk image via podman or docker (no local
#                     emscripten needed). NOTE: the build rebuilds libge.a with
#                     emcc, so run 'make clean && make' afterwards to restore the
#                     native build.
#   make wasm-run  -- serve console/wasm over HTTP and open it in a browser.
#
# Override the container runtime / image if needed, e.g.:
#   make wasm CONTAINER=docker
WASM_PORT  ?= 8120
WASM_IMAGE ?= docker.io/emscripten/emsdk
CONTAINER  ?= $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)

.PHONY: wasm
wasm:
	@if command -v emcc >/dev/null 2>&1; then \
		echo ">> building wasm with local emcc"; \
		$(MAKE) -C console/wasm; \
	elif [ -n "$(CONTAINER)" ]; then \
		echo ">> building wasm with $(CONTAINER) ($(WASM_IMAGE))"; \
		$(CONTAINER) run --rm -v "$(CURDIR)":/src $(WASM_IMAGE) make -C console/wasm; \
	else \
		echo "error: 'make wasm' needs emscripten (emcc) or a container runtime."; \
		echo "       install emscripten, or put 'podman'/'docker' on PATH."; \
		exit 1; \
	fi

.PHONY: wasm-run
wasm-run:
	@test -f console/wasm/main.mjs && test -f console/wasm/main.wasm || { \
		echo "WASM build missing — run 'make wasm' first (needs emscripten)"; \
		exit 1; }
	@echo "Serving console/wasm at http://localhost:$(WASM_PORT)/console.html (Ctrl-C to stop)"
	@( sleep 1 ; xdg-open "http://localhost:$(WASM_PORT)/console.html" >/dev/null 2>&1 || true ) &
	@python3 -m http.server $(WASM_PORT) --directory console/wasm

.PHONY: clean
clean:
	rm -f libge.a main.o ge tests/tests
	rm -f $(OBJS) $(OBJS:%.o=%.d)
	rm -f $(TESTS) $(TESTS:%.o=%.d)
	$(MAKE) -C assembler clean
	$(MAKE) -C disassembler clean
	# NB: do NOT recurse into console/wasm clean here — the wasm build's own
	# libge.a step calls `make -C ../.. clean`, so recursing would delete the
	# console/wasm/main.o it just built and break `make wasm`. Clean the wasm
	# artifacts explicitly with `make wasm-clean` / `make -C console/wasm clean`.

.PHONY: wasm-clean
wasm-clean:
	$(MAKE) -C console/wasm clean

.PHONY: docs
docs:
	cd docs ; doxygen

.PHONY: open-docs
open-docs: docs
	open docs/html/index.html

