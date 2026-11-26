#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "shell.h"

int
prompline(char *buffer, size_t capacity)
{
    int c;
    size_t length = 0;
    int invalid = 0;

    if (isatty(STDIN_FILENO)) {
        if (printf("lab12$ ") < 0 || fflush(stdout) == EOF) {
            perror("prompt");
            return -1;
        }
    }
    for (;;) {
        c = getchar();
        if (c == '\n' || c == EOF) {
            break;
        }
        if (c == '\0' || length == capacity - 1) {
            invalid = 1;
        } else if (!invalid) {
            buffer[length++] = (char)c;
        }
    }
    if (ferror(stdin)) {
        perror("stdin");
        return -1;
    }
    if (invalid) {
        fprintf(stderr, "Input line is too long or contains null bytes; discarded\n");
        length = 0;
    }
    buffer[length] = '\0';
    return c == EOF && length == 0 && !invalid ? 0 : 1;
}
