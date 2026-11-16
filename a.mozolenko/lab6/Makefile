CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
TARGET=lab6

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.o
