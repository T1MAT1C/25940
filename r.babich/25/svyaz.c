#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

int main(void)
{
    int fd[2]; //Связть fd[0] - чтение fd[1] - запись
    if(pipe(fd) == -1) //Пайп
    {
        perror("pipe");
    }

    pid_t pid = fork(); //Вызвать субпродукт

    if(pid == -1)
    {
        perror("fork");
    } else if(pid == 0) //Дочерний процесс
    {
        close(fd[0]); //Читать не умеем

        char buf[1024] = {0};

        fgets(buf, 1024, stdin);

        write(fd[1], buf, 1024);

        close(fd[1]); //Писать не нужно
    } else { //Родительский процесс
        close(fd[1]); //Писать не умеем

        char buf[1024] = {0};

        read(fd[0], buf, sizeof(buf));

        for(int i = 0; i < 1024; i++)
        {
            buf[i] = toupper(buf[i]);
        }

        printf("%s\n", buf);

        close(fd[0]); //Читать не нужно

        wait(NULL); //Просто ждём. Не важно кого.
    }

    return 0;
}
