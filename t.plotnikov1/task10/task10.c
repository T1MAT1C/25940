
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execvp(argv[1], &argv[1]);

        int err = errno;
        fprintf(stderr, "Ошибка запуска %s: %s\n",
                argv[1], strerror(err));
        _exit(err == ENOENT ? 127 : 126);
    }

    int status;

    // Ждём завершения дочернего процесса
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        printf("Command exited with status: %d\n",
               WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("Command killed by signal: %d\n",
               WTERMSIG(status));
    }

    return 0;
}