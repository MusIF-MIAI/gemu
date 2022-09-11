OBJS=main.o msl.o ge.o

ge : $(OBJS)
	cc -o ge $(OBJS)

main.o : ge.h
msl.o : ge.h msl.h
ge.o : ge.h msl.h
