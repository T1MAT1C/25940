#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    //execlp
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    pid_t pid;

    if (argc != 2)
    {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    pid = fork(); // подпроцесс

    if (pid == -1)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0) // подпроцесс
    {
        execlp("cat", "cat", argv[1], (char *)NULL);
        perror("execlp"); // если cat не запустился
        _exit(1);
    }

    printf("task9: подпроцесс запущен\n");
    fflush(stdout);

    if (waitpid(pid, NULL, 0) == -1) // Ждемcccccc...
    {
        perror("waitpid");
        return 1;
    }

    printf("task9: подпроцесс завершился\n");

    return 0;
}