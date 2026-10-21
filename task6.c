#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>

typedef struct {
    off_t offset;
    long length;
} Line;

volatile sig_atomic_t timeout = 0;

void alarm_handler(int sig)
{
    (void)sig;
    timeout = 1;
}

/* Добавление строки в таблицу */
int add_line(Line **table, int *count, off_t start, long length)
{
    Line *tmp = realloc(*table, (*count + 1) * sizeof(Line));
    if (tmp == NULL)
        return -1;

    *table = tmp;
    (*table)[*count].offset = start;
    (*table)[*count].length = length;
    (*count)++;
    return 0;
}

/* Вывод строки или всего файла */
int print_bytes(int fd, off_t start, off_t length)
{
    char buf[512];

    if (lseek(fd, start, SEEK_SET) == -1)
        return -1;

    while (length > 0)
    {
        size_t size = length > sizeof(buf) ? sizeof(buf) : (size_t)length;
        ssize_t n = read(fd, buf, size);

        if (n <= 0)
            return -1;

        ssize_t done = 0;
        while (done < n)
        {
            ssize_t k = write(1, buf + done, n - done);
            if (k <= 0)
                return -1;
            done += k;
        }

        length -= n;
    }
    return 0;
}

/* Ввод числа, таймер только на первую клавишу */
int read_number(long *number, int first)
{
    struct termios old, mode;
    char input[64], ch, *end;
    int len = 0, bad = 0, status = 1;
    ssize_t n;

    if (tcgetattr(0, &old) == -1)
        return -1;

    mode = old;
    mode.c_lflag &= ~(ICANON | ECHO);
    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSANOW, &mode) == -1)
        return -1;

    timeout = 0;
    if (first)
        alarm(5);

    while (1)
    {
        n = read(0, &ch, 1);

        if (n == -1)
        {
            if (errno == EINTR && timeout)
            {
                status = 2;
                break;
            }
            if (errno == EINTR)
                continue;

            status = -1;
            break;
        }

        if (n == 0)
        {
            status = -1;
            break;
        }

        /* Первая клавиша отключает таймер */
        if (first)
        {
            alarm(0);
            first = 0;
        }

        if (ch == '\n' || ch == '\r')
        {
            printf("\n");
            break;
        }

        if (ch == 127 || ch == '\b')
        {
            if (len > 0)
            {
                len--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (len < 63)
        {
            input[len++] = ch;
            if (isprint((unsigned char)ch))
                write(1, &ch, 1);
        }
        else
            bad = 1;
    }

    alarm(0);
    if (tcsetattr(0, TCSANOW, &old) == -1)
        return -1;

    if (status != 1)
        return status;

    input[len] = '\0';
    errno = 0;
    *number = strtol(input, &end, 10);

    if (bad || end == input || errno == ERANGE)
        return 0;

    while (isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return 0;

    return 1;
}

int main(int argc, char *argv[])
{
    int fd, count = 0, first = 1;
    char ch;
    ssize_t n;
    Line *table = NULL;
    off_t start = 0, size = 0;

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

    /* Составляем таблицу строк */
    while ((n = read(fd, &ch, 1)) == 1)
    {
        size++;

        if (ch == '\n')
        {
            if (add_line(&table, &count, start, size - start) == -1)
                goto error;

            start = size;
        }
    }

    if (n == -1)
        goto error;

    if (size > start)
        if (add_line(&table, &count, start, size - start) == -1)
            goto error;

    /* Настраиваем таймер */
    struct sigaction sa = {0};
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1)
        goto error;

    while (1)
    {
        long number;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout);

        int status = read_number(&number, first);
        first = 0;

        if (status == 2)
        {
            printf("\nВремя вышло. Весь файл:\n");
            fflush(stdout);
            if (print_bytes(fd, 0, size) == -1)
                perror("print_bytes");
            break;
        }

        if (status == -1)
            break;

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

        Line line = table[number - 1];

        if (print_bytes(fd, line.offset, line.length) == -1)
        {
            perror("print_bytes");
            break;
        }
    }

    free(table);
    close(fd);
    return 0;

error:
    perror("Ошибка");
    free(table);
    close(fd);
    return 1;
}
