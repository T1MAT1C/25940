CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
TARGET=ex_time

all: $(TARGET)

$(TARGET): ex_time.c
	$(CC) $(CFLAGS) -o $(TARGET) ex_time.c

clean:
	rm -f $(TARGET) *.o
