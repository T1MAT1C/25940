#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

typedef struct
{
    off_t offset;   // Где строка начинается в файле
    long length;    // Длина строки в байтах
} Line;

volatile sig_atomic_t timeout = 0;

void alarm_handler(int sig)
{
    (void)sig;
    timeout = 1;
}

/* функция вывода всего файла */
int print_bytes(int fd, off_t start, off_t end) {
    char buf[256];
    ssize_t n;
    if (lseek(fd, start, SEEK_SET) == (off_t)-1) return -1;
    
    while (start < end) {
        size_t to_read = (end - start > sizeof(buf)) ? sizeof(buf) : (size_t)(end - start);
        n = read(fd, buf, to_read);
        if (n <= 0) break;
        if (write(STDOUT_FILENO, buf, n) == -1) return -1;
        start += n;
    }
    return 0;
}

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
                free(table);
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

    // Настройка обработки сигнала SIGALRM вне цикла (достаточно одного раза)
    struct sigaction sa = {0};
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Не используем SA_RESTART, чтобы read прерывался!

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        close(fd);
        free(table);
        return 1;
    }

    while (1)
    {
        Line line;
        char *buffer;
        ssize_t bytes;
        char input_buf[32];
        long number;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout); // Выталкиваем текст из буфера перед ожиданием

        alarm(5); // таймер 5 секунд

        bytes = read(STDIN_FILENO, input_buf, sizeof(input_buf) - 1);
        
        alarm(0);

        // Если сработал таймер во время ожидания ввода
        if (timeout) {
            printf("\nВремя вышло. Весь файл:\n");
            off_t end_pos = lseek(fd, 0, SEEK_END);
            if (print_bytes(fd, 0, end_pos) == -1)
                perror("print_bytes");
            break;
        }

        if (bytes <= 0) break;

        input_buf[bytes] = '\0';
        
        // Парсим число из буфера вручную
        if (sscanf(input_buf, "%ld", &number) != 1) {
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
