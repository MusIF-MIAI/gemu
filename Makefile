GE_OBJS=msl.o ge.o pulse.o msl-timings.o console_socket.o peripherical.o log.o
TEST_OBJS=tests/alpha.o tests/beta.o  tests/init.o  tests/peri.o

CFLAGS+=-MD -MP
CC=gcc

ge : main.o $(GE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ge main.o $(GE_OBJS)

-include $(OBJS:%.o=%.d)

tests/tests : tests/tests.o $(GE_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ tests/tests.o $(GE_OBJS) $(TEST_OBJS) -lcheck

.PHONY: clean

clean:
	rm -f ge tests/tests
	rm -f $(GE_OBJS) $(GE_OBJS:%.o=%.d)
	rm -f $(TEST_OBJS) $(TEST_OBJS:%=%.d)

