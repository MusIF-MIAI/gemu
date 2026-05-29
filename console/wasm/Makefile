CC=emcc
AR=emar

# FS + FORCE_FILESYSTEM let the page write an uploaded .cap deck into the
# in-memory filesystem (MEMFS) so the card-reader bootstrap can read it.
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

# Containerised build (no local emscripten needed). Works with podman or docker;
# override with CONTAINER=docker / a different WASM_IMAGE if desired.
CONTAINER  ?= $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)
WASM_IMAGE ?= docker.io/emscripten/emsdk

.PHONY: docker
docker:
	$(CONTAINER) run --rm -v $(shell pwd)/../../:/src $(WASM_IMAGE) make -C console/wasm
