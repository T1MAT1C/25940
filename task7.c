#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>  // для mmap
#include <sys/stat.h>  // для fstat (чтобы узнать размер файла)

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

/* функция вывода всего файла через write(2) заменена на прямой вывод из памяти */
int print_bytes(const char *mapped_file, off_t start, off_t end) {
    if (start >= end) return 0;
    // Используем write только для вывода в stdout, как и просит задание (замена write для файла)
    if (write(STDOUT_FILENO, mapped_file + start, end - start) == -1) 
        return -1;
    return 0;
}

/* Добавляет одну строку в таблицу */
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
    struct stat sb;       // Для получения размера файла
    char *mapped_file;    // Указатель на отображенный файл в памяти

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

    // Узнаем размер файла, необходимый для mmap
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }

    // Если файл пустой, проецировать нечего
    if (sb.st_size == 0) {
        printf("Файл пуст\n");
        close(fd);
        return 0;
    }

    // Отображаем файл в память (только для чтения, разделяемое отображение)
    mapped_file = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped_file == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    /*
     * Строим таблицу строк.
     * Вместо read() просто перебираем символы в массиве mapped_file.
     */
    for (off_t i = 0; i < sb.st_size; i++) {
        char ch = mapped_file[i];
        length++;
        if (ch == '\n') {
            if (add_line(&table, &count, start, length) == -1) {
                perror("realloc");
                free(table);
                munmap(mapped_file, sb.st_size);
                close(fd);
                return 1;
            }
            // Начало следующей строки — это индекс следующего символа
            start = i + 1;
            length = 0;
        }
    }

    // Если последняя строка не заканчивается '\n'
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

    // Настройка обработки сигнала SIGALRM
    struct sigaction sa = {0};
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        munmap(mapped_file, sb.st_size);
        close(fd);
        free(table);
        return 1;
    }

    while (1)
    {
        Line line;
        ssize_t bytes;
        char input_buf[32];
        long number;

        printf("Введите номер строки (0 - выход): ");
        fflush(stdout);

        alarm(5); // таймер 5 секунд
        bytes = read(STDIN_FILENO, input_buf, sizeof(input_buf) - 1);
        alarm(0);

        if (timeout) {
            printf("\nВремя вышло. Весь файл:\n");
            if (print_bytes(mapped_file, 0, sb.st_size) == -1)
                perror("print_bytes");
            break;
        }

        if (bytes <= 0) break;

        input_buf[bytes] = '\0';
        
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

        line = table[number - 1];

        // Вместо lseek + malloc + read прямой вывод из памяти в STDOUT
        if (write(STDOUT_FILENO, mapped_file + line.offset, line.length) == -1) {
            perror("write");
            break;
        }
    }

    // Освобождаем ресурсы
    free(table);
    munmap(mapped_file, sb.st_size);
    close(fd);
    return 0;
}
