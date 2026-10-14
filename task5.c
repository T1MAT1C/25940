
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

typedef struct
{
    off_t offset;
    long length;
} Line;


/* Добавляет строку в таблицу */
int add_line(Line **table, int *count, off_t offset, long length)
{
    Line *tmp;

    tmp = realloc(*table, (*count + 1) * sizeof(Line));

    if (tmp == NULL)
        return -1;

    *table = tmp;

    (*table)[*count].offset = offset;
    (*table)[*count].length = length;

    (*count)++;

    return 0;
}


int main(int argc, char *argv[])
{
    int fd;
    char ch;
    ssize_t result;

    Line *table = NULL;
    int count = 0;

    off_t start = 0;
    long length = 0;
    long number;

    char input[100];
    char *end;
    int c;


    if (argc != 2)
    {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }


    /* Строим таблицу строк */
    while ((result = read(fd, &ch, 1)) == 1)
    {
        length++;

        if (ch == '\n')
        {
            if (add_line(&table, &count, start, length) == -1)
            {
                perror("realloc");
                free(table);
                close(fd);
                return 1;
            }

            start = lseek(fd, 0L, SEEK_CUR);

            if (start == (off_t)-1)
            {
                perror("lseek");
                free(table);
                close(fd);
                return 1;
            }

            length = 0;
        }
    }

    if (result == -1)
    {
        perror("read");
        free(table);
        close(fd);
        return 1;
    }

    /* Последняя строка без '\n' */
    if (length > 0)
    {
        if (add_line(&table, &count, start, length) == -1)
        {
            perror("realloc");
            free(table);
            close(fd);
            return 1;
        }
    }


    /* Ввод номера строки */
    while (1)
    {
        Line line;
        char *buffer;
        ssize_t bytes;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout);

        /* Читаем ввод целиком */
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        /* Если ввод слишком длинный, очищаем остаток */
        if (strchr(input, '\n') == NULL && !feof(stdin))
        {
            while ((c = getchar()) != '\n' && c != EOF)
            {
            }

            printf("Неверный ввод\n");
            continue;
        }

        /* Превращаем текст в число */
        errno = 0;
        number = strtol(input, &end, 10);

        /* Проверяем, получилось ли число */
        if (end == input || errno == ERANGE)
        {
            printf("Неверный ввод\n");
            continue;
        }

        /* Пропускаем пробелы после числа */
        while (isspace((unsigned char)*end))
            end++;

        /* Если остались лишние символы */
        if (*end != '\0')
        {
            printf("Неверный ввод\n");
            continue;
        }

        if (number == 0)
            break;

        if (number < 1 || number > count)
        {
            printf("Такой строки нет\n");
            continue;
        }

        line = table[number - 1];

        if (lseek(fd, line.offset, SEEK_SET) == (off_t)-1)
        {
            perror("lseek");
            break;
        }

        buffer = malloc(line.length + 1);

        if (buffer == NULL)
        {
            perror("malloc");
            break;
        }

        bytes = read(fd, buffer, line.length);

        if (bytes == -1)
        {
            perror("read");
            free(buffer);
            break;
        }

        buffer[bytes] = '\0';

        printf("%s", buffer);

        free(buffer);
    }

    free(table);
    close(fd);

    return 0;
}
