#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    const char *path;
    char *safe_path = NULL;
    int unordered = argc == 3 && strcmp(argv[1], "--unordered") == 0;
    pid_t child;
    int status;
    int failed = 0;

    if (argc != 2 && !unordered) {
        fprintf(stderr, "Usage: %s [--unordered] file\n", argv[0]);
        return EXIT_FAILURE;
    }
    path = argv[unordered ? 2 : 1];
    if (path[0] == '-') {
        size_t length = strlen(path);

        safe_path = malloc(length + 3);
        if (safe_path == NULL) {
            perror("malloc");
            return EXIT_FAILURE;
        }
        memcpy(safe_path, "./", 2);
        memcpy(safe_path + 2, path, length + 1);
        path = safe_path;
    }
    if (fflush(NULL) == EOF) {
        perror("fflush");
        free(safe_path);
        return EXIT_FAILURE;
    }
    child = fork();
    if (child == -1) {
        perror("fork");
        free(safe_path);
        return EXIT_FAILURE;
    }
    if (child == 0) {
        execlp("cat", "cat", path, (char *)NULL);
        perror("cat");
        _exit(127);
    }
    free(safe_path);
    if (printf("Parent: child PID is %lu\n", (unsigned long)child) < 0) {
        failed = 1;
    }
    if (unordered && puts("Parent: final line (before waiting)") == EOF) {
        failed = 1;
    }
    if (fflush(stdout) == EOF) {
        perror("fflush");
        failed = 1;
    }
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }
    if (!unordered && puts("Parent: final line (child has finished)") == EOF) {
        failed = 1;
    }
    if (fflush(stdout) == EOF) {
        perror("fflush");
        failed = 1;
    }
    return !failed && WIFEXITED(status) && WEXITSTATUS(status) == 0 ?
        EXIT_SUCCESS : EXIT_FAILURE;
}
