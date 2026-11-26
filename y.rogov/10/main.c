#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("Usage: %s [path]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  else if (pid == 0)
  {
    printf("I'm child. My output:\n");
    fflush(stdout);

    execvp(argv[1], &argv[1]);

    perror("execve");
    _exit(127);
  }
  else
  {
    siginfo_t status;
    waitid(P_PID, pid, &status, WEXITED);

    printf("I'm parent. My output:\n");
    printf("child exit code: %d\n", status.si_code);
  }
}