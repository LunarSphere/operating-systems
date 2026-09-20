CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g
SHIM_FLAGS ?= -std=c11 -Wall -Wextra -g

all: clemsh envshim.so
clemsh: clemsh.o parser.o common.o
	$(CC) $(CFLAGS) -o $@ clemsh.o parser.o common.o
clemsh.o: clemsh.c parser.h
	$(CC) $(CFLAGS) -c clemsh.c
parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c
common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c
envshim.so: envshim.c
	$(CC) $(SHIM_FLAGS) -fPIC -shared -o $@ envshim.c -ldl
run: clemsh
	./clemsh
clean:
	rm -f clemsh clemsh.o parser.o common.o envshim.so *.txt