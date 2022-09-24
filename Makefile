OBJS=main.o msl.o ge.o pulse.o msl-timings.o console_socket.o peripherical.o
CFLAGS+=-MD -MP
CC=gcc
TESTS=$(patsubst %.c,%,$(wildcard tests/*.c))

ge : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ge $(OBJS)


$(TESTS) : % : %.o $(filter-out main.o, $(OBJS))
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -lcheck -o $@

-include $(OBJS:%.o=%.d)


check : $(TESTS)
	for i in $(TESTS);do ./$$i;done


.PHONY: clean

clean:
	rm -f ge
	rm -f $(OBJS) $(OBJS:%.o=%.d)
	rm -f $(TESTS) $(TESTS:%=%.d) $(TESTS:%=%.o)

