CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c99

.PHONY: all clean

all: capcat

capcat: capcat.c ../cap.c ../transcode.c ../binimage.c
	$(CC) $(CFLAGS) -I.. -o $@ capcat.c ../cap.c ../transcode.c ../binimage.c

clean:
	rm -f capcat
