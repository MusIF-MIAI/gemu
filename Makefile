OBJS=main.o msl.o ge.o pulse.o msl-timings.o msl-commands.o
CC=gcc
TESTS=$(patsubst %.c,%,$(wildcard tests/*.c))

.PHONY : clean

ge : $(OBJS)
	$(CC) $(CFLAGS) -o ge $(OBJS)

$(TEST) : % : %.o $(filter-out main.o, $(OBJS))
	$(CC) $(CFLAGS) $^ -lcheck -o $@

main.o : ge.h
msl.o : ge.h msl.h msl-timings.h msl-states.h
ge.o : ge.h msl.h msl-timings.h msl-states.h
msl-timings.o : ge.h msl-timings.h msl-states.h
msl-commands.o : ge.h msl-commands.h

libge.so : $(OBJS)
	cc -o libge.so -shared $(OBJS)


check : $(TESTS)
	for i in $(TESTS);do ./$$i;done


.PHONY: clean

clean:
	rm -f *.o ge libge.so $(OBJS)
