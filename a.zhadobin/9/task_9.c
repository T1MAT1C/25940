
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

void print_help(void)
{
    printf("\n====================================\n");
    printf("         TWO PROCESSES\n");
    printf("====================================\n");
    printf("This program creates a child process\n");
    printf("using fork(). The child executes cat\n");
    printf("to display the contents of a file.\n");

    printf("\nCommands:\n");
    printf("  help      - show this information\n");
    printf("  filename  - file to display with cat\n");

    printf("\nUsage:\n");
    printf("  ./task_9 <filename>\n");
    printf("  ./task_9 help\n");

    printf("\nProcess behavior:\n");
    printf("  1. Parent creates a child process\n");
    printf("  2. Child executes cat using execlp\n");
    printf("  3. Parent prints messages\n");
    printf("  4. Parent waits using waitpid\n");
    printf("  5. Parent prints the final message\n");

    printf("====================================\n\n");
}

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;

    if (argc == 2 &&
        strcmp(argv[1], "help") == 0) {
        print_help();
        return 0;
    }

    print_help();

    if (argc != 2) {
        fprintf(stderr, "Error: invalid arguments!\n");
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    fflush(stdout);

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        printf("[CHILD] My PID: %ld\n", (long)getpid());
        fflush(stdout);

        execlp("cat", "cat", argv[1], (char *)NULL);

        perror("execlp");
        _exit(1);
    }

    printf("[PARENT] My PID: %ld\n", (long)getpid());
    printf("[PARENT] Child PID: %ld\n", (long)pid);

    printf("[PARENT] Printing some text while child is working...\n");
    fflush(stdout);

    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        printf("[PARENT] Child exited with status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[PARENT] Child terminated by signal: %d\n", WTERMSIG(status));
    }

    printf("[PARENT] Child has finished. This is the last line printed by parent.\n");

    return 0;
}
