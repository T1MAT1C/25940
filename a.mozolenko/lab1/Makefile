CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = lab1

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.o