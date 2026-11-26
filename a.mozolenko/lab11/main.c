#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "execvpe.h"

extern char **environ;

int
main(int argc, char *argv[])
{
    size_t inherited = 0;
    size_t count;
    size_t i;
    int separator;
    int error;
    char **environment;

    for (separator = 1; separator < argc; ++separator) {
        if (strcmp(argv[separator], "--") == 0) {
            break;
        }
        if (strchr(argv[separator], '=') == NULL || argv[separator][0] == '=') {
            break;
        }
    }
    if (separator >= argc - 1 || strcmp(argv[separator], "--") != 0) {
        fprintf(stderr, "Usage: %s [NAME=VALUE ...] -- command [argument ...]\n",
            argv[0]);
        return EXIT_FAILURE;
    }
    while (environ[inherited] != NULL) {
        ++inherited;
    }
    if ((size_t)argc > SIZE_MAX / sizeof(*environment) ||
        inherited > SIZE_MAX / sizeof(*environment) - (size_t)argc) {
        fprintf(stderr, "Environment is too large\n");
        return EXIT_FAILURE;
    }
    environment = malloc((inherited + (size_t)argc) * sizeof(*environment));
    if (environment == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    for (i = 0; i < inherited; ++i) {
        environment[i] = environ[i];
    }
    count = inherited;
    for (i = 1; i < (size_t)separator; ++i) {
        size_t name_length = (size_t)(strchr(argv[i], '=') - argv[i]);
        size_t j;

        for (j = 0; j < count; ++j) {
            if (strncmp(environment[j], argv[i], name_length) == 0 &&
                environment[j][name_length] == '=') {
                break;
            }
        }
        if (j == count) {
            ++count;
        }
        environment[j] = argv[i];
    }
    environment[count] = NULL;
    execvpe(argv[separator + 1], &argv[separator + 1], environment);
    error = errno;
    perror(argv[separator + 1]);
    free(environment);
    return error == ENOENT ? 127 : 126;
}
