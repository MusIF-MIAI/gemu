OBJS=main.o msl.o ge.o pulse.o msl-timings.o msl-commands.o console_socket.o
CFLAGS+=-MD -MP
CC=gcc
TESTS=$(patsubst %.c,%,$(wildcard tests/*.c))

ge : $(OBJS)
	$(CC) $(CFLAGS) -o ge $(OBJS)

$(TESTS) : % : %.o $(filter-out main.o, $(OBJS))
	$(CC) $(CFLAGS) $^ -lcheck -o $@

-include $(OBJS:%.o=%.d)

check : $(TESTS)
	for i in $(TESTS);do ./$$i;done


.PHONY: clean

clean:
	rm -f *.d ge $(OBJS)
