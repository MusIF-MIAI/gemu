OBJS=main.o msl.o ge.o pulse.o msl-timings.o console_socket.o peripherical.o log.o
CFLAGS+=-MD -MP
CC=gcc
TESTS=$(patsubst %.c,%.o,$(wildcard tests/*.c))

ge: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ge $(OBJS)

tests/tests: $(TESTS) $(filter-out main.o, $(OBJS))
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

-include $(OBJS:%.o=%.d)
-include $(TESTS:%.o=%.d)

.PHONY: check
check: tests/tests
	tests/tests

.PHONY: clean
clean:
	rm -f ge
	rm -f $(OBJS) $(OBJS:%.o=%.d)
	rm -f $(TESTS) $(TESTS:%.o=%.d)

