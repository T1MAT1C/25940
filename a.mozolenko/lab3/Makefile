CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
TARGET=lab3

all: $(TARGET)
	chmod 0600 data.txt

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.o
