#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

extern char **environ;

int main()
{
  pid_t pid = fork();

  if (pid < 0)
  { // error
    perror("fork");
    return EXIT_FAILURE;
  }
  else if (pid == 0)
  { // child
    setpgid(0, 0);

    printf("\nI'm child. My output:\n");
    fflush(stdout);
    
    char *name = "/usr/bin/cat";
    char *to_read = "./9/wap.txt";
    char *args[] = {name, to_read, NULL};
    execve(name, args, environ);

    perror("execve");
    exit(127);

    // execl(name, name, "./9/test.txt", NULL);
    // execv(name, args);
  }
  else 
  { // parent
    setpgid(pid, pid);
    int child_pgid = getpgid(pid);
  
    int status;
    wait(&status);

    printf("\nI'm parent. My output:\n");
    printf("my id: %d\n", getpid());
    printf("child id: %d\n", pid);
    printf("child pgid: %d\n", child_pgid);
    // printf("child raw status: %d\n", status);
  }

  return EXIT_SUCCESS;
}