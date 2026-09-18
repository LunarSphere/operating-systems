CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g

all: clemsh
clemsh: clemsh.o parser.o common.o
	$(CC) $(CFLAGS) -o $@ clemsh.o parser.o common.o
clemsh.o: clemsh.c parser.h
	$(CC) $(CFLAGS) -c clemsh.c
parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c
common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c
run: clemsh
	./clemsh
clean:
	rm -f clemsh clemsh.o parser.o common.o envshim.so