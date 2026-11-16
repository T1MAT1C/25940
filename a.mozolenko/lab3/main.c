#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

static int
check_access(const char *path)
{
    FILE *file;

    printf("Real UID: %lu, Effective UID: %lu\n",
        (unsigned long)getuid(), (unsigned long)geteuid());

    if (fflush(stdout) == EOF) {
        perror("fflush");
        return -1;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return -1;
    }

    if (fclose(file) == EOF) {
        perror("fclose");
        return -1;
    }

    printf("Opened and closed '%s' successfully\n", path);
    return 0;
}

int
main(int argc, char *argv[])
{
    const char *path;
    int status = EXIT_SUCCESS;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    path = argc == 2 ? argv[1] : "data.txt";

    puts("Before setuid:");
    if (check_access(path) == -1) {
        status = EXIT_FAILURE;
    }

    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    puts("After setuid:");
    if (check_access(path) == -1) {
        status = EXIT_FAILURE;
    }

    if (fflush(stdout) == EOF) {
        perror("fflush");
        return EXIT_FAILURE;
    }

    return status;
}
