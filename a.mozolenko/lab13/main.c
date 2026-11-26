#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_SIZE 4096
#define MAX_ARGS 128

static int
parse(char *line, char **args)
{
    char *src = line;
    char *dst = line;
    size_t count = 0;

    while (*src != '\0') {
        while (isspace((unsigned char)*src)) {
            ++src;
        }
        if (*src == '\0') {
            break;
        }
        if (count == MAX_ARGS) {
            fprintf(stderr, "Too many arguments\n");
            return -1;
        }
        args[count++] = dst;
        while (*src != '\0' && !isspace((unsigned char)*src)) {
            if (strchr(";&<>|", *src) != NULL) {
                fprintf(stderr, "Only simple commands are supported\n");
                return -1;
            }
            if (*src == '\'' || *src == '"') {
                char quote = *src++;

                while (*src != '\0' && *src != quote) {
                    if (quote == '"' && *src == '\\') {
                        if (*++src == '\0') {
                            goto bad_quote;
                        }
                    }
                    *dst++ = *src++;
                }
                if (*src == '\0') {
                    goto bad_quote;
                }
                ++src;
            } else {
                if (*src == '\\' && *++src == '\0') {
                    goto bad_quote;
                }
                *dst++ = *src++;
            }
        }
        if (*src != '\0') {
            ++src;
        }
        *dst++ = '\0';
    }
    args[count] = NULL;
    return (int)count;

bad_quote:
    fprintf(stderr, "Unclosed quote or trailing escape\n");
    return -1;
}

static int
read_line(char *buffer)
{
    size_t length = 0;
    int c;
    int invalid = 0;

    if (isatty(STDIN_FILENO) &&
        (printf("lab13$ ") < 0 || fflush(stdout) == EOF)) {
        perror("prompt");
        return -1;
    }
    for (;;) {
        c = getchar();
        if (c == '\n' || c == EOF) {
            break;
        }
        if (c == '\0' || length == INPUT_SIZE - 1) {
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

int
main(void)
{
    char buffer[INPUT_SIZE];
    char *args[MAX_ARGS + 1];
    int input;
    int last_status = 0;

    if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Unable to disable stdin buffering\n");
        return EXIT_FAILURE;
    }
    while ((input = read_line(buffer)) > 0) {
        pid_t child;
        int status;
        int count = parse(buffer, args);

        if (count <= 0) {
            if (count < 0) {
                last_status = 2;
            }
            continue;
        }
        if (strcmp(args[0], "exit") == 0 && count == 1) {
            break;
        }
        if (fflush(NULL) == EOF) {
            perror("fflush");
            return EXIT_FAILURE;
        }
        child = fork();
        if (child == -1) {
            perror("fork");
            last_status = 1;
            continue;
        }
        if (child == 0) {
            int error;

            execvp(args[0], args);
            error = errno;
            perror(args[0]);
            _exit(error == ENOENT ? 127 : 126);
        }
        while (waitpid(child, &status, 0) == -1) {
            if (errno != EINTR) {
                perror("waitpid");
                return EXIT_FAILURE;
            }
        }
        last_status = WIFEXITED(status) ? WEXITSTATUS(status) :
            128 + WTERMSIG(status);
        if (last_status > 255) {
            last_status = 1;
        }
    }
    return input < 0 ? EXIT_FAILURE : last_status;
}
