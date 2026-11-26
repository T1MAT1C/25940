CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
TARGET=lab12
OBJS=shell.o promptline.o parseline.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

shell.o: shell.c shell.h
promptline.o: promptline.c shell.h
parseline.o: parseline.c shell.h

clean:
	rm -f $(TARGET) *.o
