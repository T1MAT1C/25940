#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/wait.h>
#include <string.h>

int main()
{
  int pipefd[2];
  if (pipe(pipefd) < 0) 
    err(EXIT_FAILURE, "pipe");

  pid_t pid = fork();
  if (pid < 0)
    err(EXIT_FAILURE, "fork");
  if (pid == 0)
  { // CHILD reads
    if (close(pipefd[1]) < 0)
      err(EXIT_FAILURE, "close");

    char buf[256] = {0};
    int n;
    if ((n = read(pipefd[0], &buf, sizeof buf)) < 0)
      err(EXIT_FAILURE, "read");
    for (int i = 0; i < n; ++i) buf[i] = toupper(buf[i]);

    if (write(STDOUT_FILENO, "CHILD: GOT MESSAGE:\n", 21) < 0)
      err(EXIT_FAILURE, "write");
    if (write(STDOUT_FILENO, buf, strlen(buf)) < 0)
      err(EXIT_FAILURE, "write");
    if (write(STDOUT_FILENO, "\n", 1) != 1)
      err(EXIT_FAILURE, "write");
    if (close(pipefd[0]) < 0)
      err(EXIT_FAILURE, "close");

    _exit(EXIT_SUCCESS);
  }
  else
  { // PARENT writes
    if (close(pipefd[0]) < 0)
      err(EXIT_FAILURE, "close");
    if (write(pipefd[1], "Get this Message from Parent.", 29) < 0)
      err(EXIT_FAILURE, "write");
    if (write(STDOUT_FILENO, "PARENT: SENT MESSAGE:\n", 22) < 0)
      err(EXIT_FAILURE, "write");
    if (write(STDOUT_FILENO, "Get this Message from Parent.", 29) < 0)
      err(EXIT_FAILURE, "write");
    if (write(STDOUT_FILENO, "\n", 1) != 1)
      err(EXIT_FAILURE, "write");
    if (close(pipefd[1]) < 0)
      err(EXIT_FAILURE, "close");
    if (wait(NULL) < 0)
      err(EXIT_FAILURE, "wait");

    exit(EXIT_SUCCESS);
  }
}
