#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct
{
    off_t offset;
    long length;
} Line;

volatile sig_atomic_t timeout = 0;

/* Обработчик сигнала */
void alarm_handler(int sig)
{
    (void)sig;
    timeout = 1;
}

/* Добавление строки в таблицу */
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

/* Вывод данных из mmap */
int print_bytes(const char *mapped_file, off_t start, off_t end)
{
    while (start < end)
    {
        ssize_t n = write(STDOUT_FILENO,
                          mapped_file + start,
                          (size_t)(end - start));

        if (n <= 0)
            return -1;

        start += n;
    }

    return 0;
}

/* Ввод числа с таймером до первой клавиши */
int read_number(long *number)
{
    char input[64];
    char ch;
    char *end;
    int len = 0;
    int too_long = 0;
    int status = 0;
    ssize_t bytes;

    struct termios old_term;
    struct termios new_term;

    /* Сохраняем настройки терминала */
    if (tcgetattr(STDIN_FILENO, &old_term) == -1)
    {
        perror("tcgetattr");
        return -1;
    }

    new_term = old_term;

    /* Читаем символы сразу, не ожидая Enter */
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 1;
    new_term.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    timeout = 0;
    alarm(5);

    while (1)
    {
        bytes = read(STDIN_FILENO, &ch, 1);

        if (timeout)
        {
            status = 2;
            break;
        }

        if (bytes == 0)
        {
            status = -1;
            break;
        }

        if (bytes == -1)
        {
            if (errno == EINTR)
                continue;

            perror("read");
            status = -1;
            break;
        }

        /* Первая клавиша нажата - выключаем таймер */
        if (len == 0 && !too_long)
            alarm(0);

        /* Enter завершает ввод */
        if (ch == '\n' || ch == '\r')
        {
            printf("\n");
            fflush(stdout);
            status = 1;
            break;
        }

        /* Обработка Backspace */
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

        /* Сохраняем символ */
        if (len < (int)sizeof(input) - 1)
        {
            input[len++] = ch;

            /* Показываем введённый символ */
            write(STDOUT_FILENO, &ch, 1);
        }
        else
        {
            too_long = 1;
        }
    }

    alarm(0);

    /* Возвращаем обычный режим терминала */
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    if (status == 2)
        return 2;

    if (status == -1)
        return -1;

    if (too_long)
        return 0;

    input[len] = '\0';

    /* Преобразуем ввод в число */
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

    return 1;
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
    struct sigaction sa = {0};

    if (argc != 2)
    {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    /* Открываем файл */
    fd = open(argv[1], O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    /* Узнаём размер файла */
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

    /* Отображаем файл в память через mmap */
    mapped_file = mmap(NULL, sb.st_size,
                       PROT_READ, MAP_SHARED, fd, 0);

    if (mapped_file == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return 1;
    }

    /* Создаём таблицу строк */
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

    /* Настраиваем обработчик SIGALRM */
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

    /* Основной цикл */
    while (1)
    {
        Line line;
        long number;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout);

        int status = read_number(&number);

        /* 5 секунд никто ничего не нажимал */
        if (status == 2)
        {
            printf("\nВремя вышло. Весь файл:\n");
            fflush(stdout);

            if (print_bytes(mapped_file, 0, sb.st_size) == -1)
                perror("write");

            break;
        }

        /* Конец ввода или ошибка */
        if (status == -1)
            break;

        /* Неправильный ввод */
        if (status == 0)
        {
            printf("Неверный ввод\n");
            continue;
        }

        /* Выход по нулю */
        if (number == 0)
            break;

        /* Проверяем номер строки */
        if (number < 1 || number > count)
        {
            printf("Такой строки нет\n");
            continue;
        }

        /* Получаем нужную строку */
        line = table[number - 1];

        /* Выводим строку напрямую из памяти */
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
