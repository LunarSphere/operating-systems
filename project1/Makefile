CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g

all: clemsh

clemsh: clemsh.o parser.o
	$(CC) $(CFLAGS) -o $@ clemsh.o parser.o

clemsh.o: clemsh.c parser.h
	$(CC) $(CFLAGS) -c clemsh.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

run: clemsh
	./clemsh

clean:
	rm -f clemsh clemsh.o parser.o envshim.so