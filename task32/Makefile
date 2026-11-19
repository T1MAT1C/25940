CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lsocket -lnsl

all: server32 client32

server32: server32.c
	$(CC) $(CFLAGS) server32.c -o server32 $(LIBS)

client32: client32.c
	$(CC) $(CFLAGS) client32.c -o client32 $(LIBS)

clean:
	rm -f server32 client32

.PHONY: all clean