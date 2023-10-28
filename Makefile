OBJS=msl.o ge.o pulse.o msl-timings.o console.o console_socket.o peripherical.o log.o reader.o
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

.PHONY: check
check: tests/tests
	tests/tests

.PHONY: clean
clean:
	rm -f libge.a main.o ge tests/tests
	rm -f $(OBJS) $(OBJS:%.o=%.d)
	rm -f $(TESTS) $(TESTS:%.o=%.d)

.PHONY: docs
docs:
	cd docs ; doxygen

.PHONY: open-docs
open-docs: docs
	open docs/html/index.html

