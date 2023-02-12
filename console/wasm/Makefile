CC=emcc
AR=emar

CFLAGS=-I../.. -s EXPORTED_RUNTIME_METHODS=["callMain"]

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

.PHONY: docker
docker:
	docker run --rm -v $(shell pwd)/../../:/src emscripten/emsdk make -C console/wasm
