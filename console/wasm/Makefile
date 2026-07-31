CC=emcc
AR=emar

# FS + FORCE_FILESYSTEM let the page write a chosen .cap deck into the in-memory
# filesystem (MEMFS) so the card-reader bootstrap can read it. That file is the
# page's only input: the deck comes from the operator's own disk, exactly as
# `ge deck.cap` takes it from the command line, so nothing is vendored here.
CFLAGS=-I../.. -sEXPORTED_RUNTIME_METHODS=callMain,FS,ccall -sFORCE_FILESYSTEM=1

LIBGE=../../libge.a

main: main.o $(LIBGE)
	$(CC) $(CFLAGS) -o $@.mjs $^ $(LIBGE)

.PHONY: $(LIBGE)
$(LIBGE):
	make -C ../.. clean
	make -C ../.. libge.a CC=$(CC) AR=$(AR)

.PHONY: clean
clean:
	rm -f main.o main.js main.wasm main.mjs
	rm -rf Site_Acceptance_Test

# Containerised build (no local emscripten needed). Works with podman or docker;
# override with CONTAINER=docker / a different WASM_IMAGE if desired.
CONTAINER  ?= $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)
WASM_IMAGE ?= docker.io/emscripten/emsdk

.PHONY: docker
docker:
	$(CONTAINER) run --rm -v $(shell pwd)/../../:/src $(WASM_IMAGE) make -C console/wasm
