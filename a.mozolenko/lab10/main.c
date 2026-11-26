#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    pid_t child;
    int status;
    int code;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s command [argument ...]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (fflush(NULL) == EOF) {
        perror("fflush");
        return EXIT_FAILURE;
    }
    child = fork();
    if (child == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (child == 0) {
        int error;

        execvp(argv[1], &argv[1]);
        error = errno;
        perror(argv[1]);
        _exit(error == ENOENT ? 127 : 126);
    }
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }
    if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
        if (printf("Exit status: %d\n", code) < 0) {
            return EXIT_FAILURE;
        }
    } else if (WIFSIGNALED(status)) {
        if (printf("Terminated by signal: %d\n", WTERMSIG(status)) < 0) {
            return EXIT_FAILURE;
        }
        code = 128 + WTERMSIG(status);
        if (code > 255) {
            code = EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Unexpected child status\n");
        return EXIT_FAILURE;
    }
    if (fflush(stdout) == EOF) {
        perror("fflush");
        return EXIT_FAILURE;
    }
    return code;
}
