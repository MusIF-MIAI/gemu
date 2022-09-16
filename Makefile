OBJS=main.o msl.o ge.o pulse.o msl-timings.o msl-commands.o
CC=gcc
.PHONY : clean

ge : $(OBJS)
	$(CC) $(CFLAGS) -o ge $(OBJS)

main.o : ge.h
msl.o : ge.h msl.h msl-timings.h msl-states.h
ge.o : ge.h msl.h msl-timings.h msl-states.h
msl-timings.o : ge.h msl-timings.h msl-states.h
msl-commands.o : ge.h msl-commands.h
clean :
	rm $(OBJS)
