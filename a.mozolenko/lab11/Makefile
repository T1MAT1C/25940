CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
TARGET=lab11

all: $(TARGET)

$(TARGET): main.c execvpe.c execvpe.h
	$(CC) $(CFLAGS) -o $(TARGET) main.c execvpe.c

clean:
	rm -f $(TARGET) *.o
