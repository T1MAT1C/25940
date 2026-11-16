#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    int fd[2];

    if(pipe(fd) == -1)
    {
        perror("pipe");
        return 1;
    }

    printf("Здесь могла быть ваша реклама:\n");
    fflush(stdout);

    pid_t pid = fork();

    if(pid == -1) //Ошибка
    {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return 1;
    } else if(pid == 0) { //Дочерний процесс
        printf("И она здесь есть (наверное):\n");
        fflush(stdout);
        
        close(fd[0]);

        if(dup2(fd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2");
            _exit(1);
        }

        close(fd[1]);

        int e = execlp("cat", "cat", "longfile.txt", NULL);

        if(e == -1)
        {
            perror("execlp");
            _exit(1);
        }
    } else { //Родительский процесс
        close(fd[1]);

        FILE *stream = fdopen(fd[0], "r");

        if(stream == NULL)
        {
            perror("fdopen");
            close(fd[0]);
            waitpid(pid, NULL, 0);
            return 1;
        }

        char *line = NULL;
        char *last = NULL;
        size_t cap = 0;

        while(getline(&line, &cap, stream) != -1)
        {
            fputs(line, stdout); // Выводим весь файл в терминал

            char *tmp = strdup(line);

            if(tmp == NULL)
            {
                perror("strdup");
                break;
            }

            free(last);
            last = tmp;
        }

        if(last != NULL)
        {
            printf("Последняя строка: %s", last);

            if(last[strlen(last) - 1] != '\n')
            {
                putchar('\n');
            }
        }

        free(line);
        free(last);
        fclose(stream);

        waitpid(pid, NULL, 0);

        printf("РЕКлАМА ХЗ\n");
    }

    return 0;
}



/*
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

int main(void)
{
    printf("Здесь могла быть ваша реклама:\n");

    pid_t pid = fork();

    if(pid == -1)
    {
        perror("fork");
    } else if(pid == 0)
    {
        printf("И она здесь есть (наверное):\n");

        int e = execlp("cat", "cat", "longfile.txt", NULL); //Исполняет бинарник cat в PATH в виде cat на longfile.txt

        if(e == -1)
        {
            perror("execlp");
            _exit(1);
        } 
    } else {
        waitpid(pid, NULL, 0);

        printf("РЕКлАМА ХЗ\n");
    }

    return 0;
}
*/