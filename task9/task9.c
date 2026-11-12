#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char argv[])
{
    pid_t pid;
    int fd;
    char input[1024];
    const charfilename = NULL;

    if (argc > 2)
    {
        printf("Использование: %s [файл]\n", argv[0]);
        return 1;
    }

    if (argc == 2)
        filename = argv[1];

    // Проверяем существование файла
    while (1)
    {
        // если имя файла не указано
        if (filename == NULL)
        {
            printf("Введите имя файла: ");
            fflush(stdout);

            if (fgets(input, sizeof(input), stdin) == NULL)
                return 0;

            // удаление ентер
            input[strcspn(input, "\n")] = '\0';

            filename = input;
        }

        fd = open(filename, O_RDONLY);

        if (fd == -1)
        {
            if (errno == ENOENT || errno == ENOTDIR)
                printf("Файл "%s" не существует\n", filename);
            else
                perror("open");

            filename = NULL;
            continue; // Повторяем ввод
        }

        break; // Файл успешно открыт
    }

    // дочерний процесс
    pid = fork();

    if (pid == -1)
    {
        perror("fork");
        close(fd);
        return 1;
    }

    if (pid == 0)
    {
        // Передаем открытый файл на вход команды cat
        if (dup2(fd, STDIN_FILENO) == -1)
        {
            perror("dup2");
            _exit(1);
        }

        if (fd != STDIN_FILENO)
            close(fd);

        execlp("cat", "cat", (char *)NULL);

        perror("execlp");
        _exit(1);
    }

    close(fd);

    printf("task9: подпроцесс запущен\n");
    fflush(stdout);

    // Ждем завершения дочернего процесса
    if (waitpid(pid, NULL, 0) == -1)
    {
        perror("waitpid");
        return 1;
    }

    printf("task9: подпроцесс завершился\n");

    return 0;
}