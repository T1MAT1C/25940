#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct
{
    off_t offset;   // Где строка начинается в файле
    long length;    // Длина строки в байтах
} Line;


/* Добавляет одну строку в таблицу */
int add_line(Line **table, int *count, off_t offset, long length)
{
    Line *tmp;
    tmp = realloc(*table, (*count + 1) * sizeof(Line)); // память под новую строку
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


    if (argc != 2)
    {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }


    /* Открываем файл только для чтения */
    fd = open(argv[1], O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }


    /*
     * Строим таблицу строк.
     * Читаем файл по одному символу.
     */
    while ((result = read(fd, &ch, 1)) == 1)
    {
        length++;

        if (ch == '\n')
        {
            if (add_line(&table, &count, start, length) == -1)
            {
                perror("realloc");
                close(fd);
                return 1;
            }

            // Текущая позиция файла теперь начало следующей строки
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


    // Если последняя строка не заканчивается '\n'
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


    /*
     * Теперь таблица готова.
     * Пользователь вводит номер нужной строки.
     */
    while (1)
    {
        Line line;
        char *buffer;
        ssize_t bytes;

        printf("Введите номер строки (0 - выход): ");

        if (scanf("%ld", &number) != 1)
        {
            printf("Неверный ввод\n");
            break;
        }


        if (number == 0)
            break;


        if (number < 1 || number > count)
        {
            printf("Такой строки нет\n");
            continue;
        }


        // В массиве нумерация начинается с нуля
        line = table[number - 1];


        // Переходим точно к началу нужной строки
        if (lseek(fd, line.offset, SEEK_SET) == (off_t)-1)
        {
            perror("lseek");
            break;
        }


        // +1 для символа конца строки '\0'
        buffer = malloc(line.length + 1);

        if (buffer == NULL)
        {
            perror("malloc");
            break;
        }


        // Читаем ровно нужную строку
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
