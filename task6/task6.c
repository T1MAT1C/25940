
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>

typedef struct {
    off_t start, len;
} Line;

Line *lines = NULL;
size_t count = 0;
volatile sig_atomic_t timeout = 0;

void on_alarm(int sig) {
    (void)sig;
    timeout = 1;
}

void add_line(off_t start, off_t len) {
    Line *p = realloc(lines, (count + 1) * sizeof(Line));
    if (!p) {
        perror("realloc");
        exit(1);
    }
    lines = p;
    lines[count++] = (Line){start, len};
}

void show_all(int fd) {
    char buf[4096];
    ssize_t n;

    lseek(fd, 0, SEEK_SET);
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, n, stdout);
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

    // Создаём таблицу строк
    char c;
    off_t pos = 0, start = 0;
    ssize_t r;

    while ((r = read(fd, &c, 1)) == 1) {
        if (c == '\n') {
            add_line(start, pos - start);
            start = pos + 1;
        }
        pos++;
    }
    if (r < 0) {
        perror("read");
        close(fd);
        free(lines);
        return 1;
    }
    if (pos > start)
        add_line(start, pos - start);

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
                show_all(fd);
            }
            break;
        }

        alarm(0);
        first = 0;

        if (timeout) {
            printf("\nВремя вышло! Весь файл:\n");
            show_all(fd);
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

        // Выводим выбранную строку
        Line line = lines[num - 1];
        if (lseek(fd, line.start, SEEK_SET) == -1) {
            perror("lseek");
            break;
        }

        for (off_t i = 0; i < line.len; i++) {
            if (read(fd, &c, 1) != 1)
                break;
            putchar(c);
        }
        putchar('\n');
    }

    close(fd);
    free(lines);
    return 0;
}