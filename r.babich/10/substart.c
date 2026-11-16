#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

int main(int _argc, char *argv[])
{
    pid_t pid = fork();

    if(pid == -1)
    {
        perror("fork");
    } else if(pid == 0)
    {
        int e = execvp(argv[1], &argv[1]); //во втором 1, а не 2, т.к. UB

        if(e == -1)
        {
            perror("execlp");
            _exit(1);
        } 
    } else {
        int end = -993;

        int e = waitpid(pid, &end, 0);

        if(e == -1)
        {
            perror("waitpid");
        } else if(WIFEXITED(end))
        {
            printf("E: %d\n", WEXITSTATUS(end));
        }
    }

    return 0;
}
