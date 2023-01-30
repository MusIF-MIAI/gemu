CC=emcc
AR=emar

CFLAGS=-I../..
LIBGE=../../libge.a

main: main.o $(LIBGE)
	$(CC) -o $@.html $^ $(LIBGE)

.PHONY: $(LIBGE)
$(LIBGE):
	make -C ../.. clean
	make -C ../.. libge.a CC=$(CC) AR=$(AR)

.PHONY: clean
clean:
	rm -f main.o main.js main.wasm main.html
