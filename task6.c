
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

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
            return 2; // Время вышло
        }

        if (bytes == 0)
        {
            if (len == 0)
            {
                alarm(0);
                return -1; // Конец ввода
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

    /* Превращаем текст в число */
    errno = 0;
    *number = strtol(input, &end, 10);

    if (end == input || errno == ERANGE)
        return 0;

    /* Пропускаем пробелы */
    while (isspace((unsigned char)*end))
        end++;

    /* Проверяем лишние символы */
    if (*end != '\0')
        return 0;

    return 1; // Правильный ввод
}

/* Вывод всего файла */
int print_bytes(int fd, off_t start, off_t end)
{
    char buf[256];
    ssize_t n;

    if (lseek(fd, start, SEEK_SET) == (off_t)-1)
        return -1;

    while (start < end)
    {
        size_t to_read = (end - start > sizeof(buf))
                         ? sizeof(buf) : (size_t)(end - start);

        n = read(fd, buf, to_read);

        if (n <= 0)
            return -1;

        ssize_t written = 0;

        while (written < n)
        {
            ssize_t w = write(STDOUT_FILENO,
                              buf + written, n - written);

            if (w <= 0)
                return -1;

            written += w;
        }

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
    char ch;
    ssize_t result;
    Line *table = NULL;
    int count = 0;
    off_t start = 0;
    long length = 0;

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

    /* Настройка сигнала */
    struct sigaction sa = {0};

    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        free(table);
        close(fd);
        return 1;
    }

    /* Ввод номеров строк */
    while (1)
    {
        Line line;
        char *buffer;
        ssize_t bytes;
        long number;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout);

        int status = read_number(&number);

        /* Если прошло 5 секунд */
        if (status == 2)
        {
            printf("\nВремя вышло. Весь файл:\n");
            fflush(stdout);

            off_t end_pos = lseek(fd, 0, SEEK_END);

            if (end_pos == (off_t)-1 ||
                print_bytes(fd, 0, end_pos) == -1)
            {
                perror("Вывод файла");
            }

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
