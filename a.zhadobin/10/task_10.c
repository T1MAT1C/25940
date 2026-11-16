
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

void print_help(void)
{
    printf("\n====================================\n");
    printf("        COMMAND EXIT STATUS\n");
    printf("====================================\n");
    printf("This program executes a command\n");
    printf("in a child process using fork()\n");
    printf("and execvp().\n");
    printf("The parent waits for the command\n");
    printf("and prints its exit status.\n");

    printf("\nUsage:\n");
    printf("  ./task_10 <command> [arguments]\n");
    printf("  ./task_10 help\n");

    printf("\nCommands:\n");
    printf("  help     - show this information\n");
    printf("  command  - executable to run\n");

    printf("\nExamples:\n");
    printf("  ./task_10 ls -l /tmp\n");
    printf("  ./task_10 grep test file.txt\n");
    printf("  ./task_10 false\n");

    printf("\nRules:\n");
    printf("  Command must be executable\n");
    printf("  All arguments are passed unchanged\n");
    printf("  Exit code 0 usually means success\n");
    printf("  Exit code 127 means execvp failed\n");
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

    if (argc < 2) {
        fprintf(stderr,"Error: command is not specified!\n");
        fprintf(stderr,"Usage: %s <command> [arguments]\n",argv[0]);
        return 1;
    }

    printf("[PARENT] Starting command: %s\n", argv[1]);

    fflush(stdout);

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execvp(argv[1], &argv[1]);
        perror("Execvp failed");
        _exit(1);
    }


    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        printf("[PARENT] Command exited normally with code: %d\n", WEXITSTATUS(status));

    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("[PARENT] Command terminated by signal: %d (%s)\n", sig, strsignal(sig));
    }

    return 0;
}
