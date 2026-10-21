
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>

typedef struct {
    size_t start, len;
} Line;

Line *lines = NULL;
size_t count = 0;
volatile sig_atomic_t timeout = 0;

void on_alarm(int sig) {
    (void)sig;
    timeout = 1;
}

void add_line(size_t start, size_t len) {
    Line *p = realloc(lines, (count + 1) * sizeof(Line));
    if (!p) {
        perror("realloc");
        exit(1);
    }
    lines = p;
    lines[count++] = (Line){start, len};
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Получаем размер файла
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }

    size_t size = st.st_size;
    char *data = NULL;

    // Отображаем файл в память
    if (size > 0) {
        data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return 1;
        }
    }
    close(fd);

    // Создаём таблицу строк
    size_t start = 0;

    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            add_line(start, i - start);
            start = i + 1;
        }
    }
    if (start < size)
        add_line(start, size - start);

    // Настраиваем сигнал таймера
    struct sigaction sa = {0};
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    char input[64];
    int first = 1;

    while (1) {
        printf("Номер строки (0 - выход): ");
        fflush(stdout);

        if (first)
            alarm(5);

        if (!fgets(input, sizeof(input), stdin)) {
            if (timeout) {
                printf("\nВремя вышло! Весь файл:\n");
                if (size)
                    fwrite(data, 1, size, stdout);
            }
            break;
        }

        alarm(0);
        first = 0;

        if (timeout) {
            printf("\nВремя вышло! Весь файл:\n");
            if (size)
                fwrite(data, 1, size, stdout);
            break;
        }

        char *end;
        long num = strtol(input, &end, 10);

        if (end == input || (*end && *end != '\n')) {
            puts("Некорректный ввод");
            continue;
        }

        if (num == 0)
            break;

        if (num < 1 || (size_t)num > count) {
            printf("Всего строк: %zu\n", count);
            continue;
        }

        // Выводим строку из памяти
        Line line = lines[num - 1];
        fwrite(data + line.start, 1, line.len, stdout);
        putchar('\n');
    }

    if (size)
        munmap(data, size);

    free(lines);
    return 0;
}
