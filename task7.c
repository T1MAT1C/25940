
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct
{
    off_t offset;
    long length;
} Line;

volatile sig_atomic_t timeout = 0;

/* Обработчик таймера */
void alarm_handler(int sig)
{
    (void)sig;
    timeout = 1;
}

/* Ввод номера строки с таймером */
int read_number(long *number)
{
    char input[64];
    char ch;
    char *end;
    ssize_t bytes;
    int len = 0;
    int too_long = 0;

    timeout = 0;
    alarm(5);

    while (1)
    {
        bytes = read(STDIN_FILENO, &ch, 1);

        if (timeout)
        {
            alarm(0);
            return 2;
        }

        if (bytes == 0)
        {
            if (len == 0)
            {
                alarm(0);
                return -1;
            }
            break;
        }

        if (bytes == -1)
        {
            if (errno == EINTR)
                continue;

            perror("read");
            alarm(0);
            return -1;
        }

        if (ch == '\n')
            break;

        if (len < sizeof(input) - 1)
            input[len++] = ch;
        else
            too_long = 1;
    }

    alarm(0);

    if (too_long)
        return 0;

    input[len] = '\0';

    errno = 0;
    *number = strtol(input, &end, 10);

    if (end == input || errno == ERANGE)
        return 0;

    while (isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return 0;

    return 1;
}

/* Вывод байтов из памяти */
int print_bytes(const char *mapped_file, off_t start, off_t end)
{
    while (start < end)
    {
        ssize_t n = write(STDOUT_FILENO,
                          mapped_file + start,
                          end - start);

        if (n <= 0)
            return -1;

        start += n;
    }

    return 0;
}

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
    Line *table = NULL;
    int count = 0;
    off_t start = 0;
    long length = 0;

    struct stat sb;
    char *mapped_file;

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

    /* Получаем размер файла */
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (sb.st_size == 0)
    {
        printf("Файл пуст\n");
        close(fd);
        return 0;
    }

    /* Отображаем файл в память */
    mapped_file = mmap(NULL, sb.st_size,
                       PROT_READ, MAP_SHARED, fd, 0);

    if (mapped_file == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return 1;
    }

    /* Строим таблицу строк */
    for (off_t i = 0; i < sb.st_size; i++)
    {
        char ch = mapped_file[i];
        length++;

        if (ch == '\n')
        {
            if (add_line(&table, &count, start, length) == -1)
            {
                perror("realloc");
                free(table);
                munmap(mapped_file, sb.st_size);
                close(fd);
                return 1;
            }

            start = i + 1;
            length = 0;
        }
    }

    /* Последняя строка без '\n' */
    if (length > 0)
    {
        if (add_line(&table, &count, start, length) == -1)
        {
            perror("realloc");
            free(table);
            munmap(mapped_file, sb.st_size);
            close(fd);
            return 1;
        }
    }

    /* Настройка сигнала */
    struct sigaction sa = {0};

    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        free(table);
        munmap(mapped_file, sb.st_size);
        close(fd);
        return 1;
    }

    /* Ввод номеров строк */
    while (1)
    {
        Line line;
        long number;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout);

        int status = read_number(&number);

        /* Если прошло 5 секунд */
        if (status == 2)
        {
            printf("\nВремя вышло. Весь файл:\n");
            fflush(stdout);

            if (print_bytes(mapped_file, 0, sb.st_size) == -1)
                perror("print_bytes");

            break;
        }

        /* Конец ввода */
        if (status == -1)
            break;

        /* Неправильный ввод */
        if (status == 0)
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

        /* Выводим строку прямо из памяти */
        if (print_bytes(mapped_file,
                        line.offset,
                        line.offset + line.length) == -1)
        {
            perror("write");
            break;
        }
    }

    /* Освобождаем память */
    free(table);
    munmap(mapped_file, sb.st_size);
    close(fd);

    return 0;
}
