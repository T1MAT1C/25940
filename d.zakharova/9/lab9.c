#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>

int main()
{

 pid_t pid, wpid;

 int status;

 if ((pid = fork()) == 0)
  execl("/bin/cat", "cat", "/etc/passwd", (char*)0);

 wpid = wait(&status);

 printf("cat status: %d\n", WEXITSTATUS(status));

 return 0;

}