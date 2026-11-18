
CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LIBS = -lsocket -lnsl

all: server31 client31

server31: server31.c
	$(CC) $(CFLAGS) server31.c -o server31 $(LIBS)

client31: client31.c
	$(CC) $(CFLAGS) client31.c -o client31 $(LIBS)

clean:
	rm -f server31 client31

.PHONY: all clean