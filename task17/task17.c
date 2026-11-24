
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

#define MAX_LINE 40
#define CTRL_W 23
#define CTRL_D 4
#define CTRL_G 7

// Удаляем последний символ, при необходимости возвращаясь на предыдущую строку
void erase_last(size_t *len) {
    if (*len == 0)
        return;

    write(STDOUT_FILENO, "\b \b", 3);
    (*len)--;

    if (*len > 0 && *len % MAX_LINE == 0)
        write(STDOUT_FILENO, "\033[1A\033[41G", 9);
}

int main(void) {
    struct termios oldt, newt;
    struct winsize ws;

    // Проверяем ширину терминала
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_col > 0 && ws.ws_col <= MAX_LINE) {
        fprintf(stderr, "Ширина терминала должна быть больше 40 столбцов\n");
        return 1;
    }

    if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
        perror("tcgetattr");
        return 1;
    }

    newt = oldt;

    // Отключаем эхо, каноническую обработку и сигналы
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {
        perror("tcsetattr");
        return 1;
    }

    size_t len = 0, capacity = 128;
    char *line = malloc(capacity);
    int esc = 0;
    unsigned char ch;

    if (!line) {
        perror("malloc");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return 1;
    }

    printf("Введите текст (Ctrl+D для выхода):\n");
    fflush(stdout);

    while (1) {
        ssize_t n = read(STDIN_FILENO, &ch, 1);

        if (n < 0 && errno == EINTR)
            continue;
        if (n != 1)
            break;

        // Игнорируем стрелки и Escape-последовательности
        if (esc == 1) {
            esc = 0;
            if (ch == '[') {
                esc = 2;
                continue;
            }
            if (ch == 'O') {
                esc = 3;
                continue;
            }
        }

        if (esc == 2) {
            if (ch >= 0x40 && ch <= 0x7E)
                esc = 0;
            continue;
        }

        if (esc == 3) {
            esc = 0;
            continue;
        }

        if (ch == 27) {
            esc = 1;
            continue;
        }

        // ERASE стирает последний символ
        if (ch == oldt.c_cc[VERASE] || ch == 8 || ch == 127) {
            erase_last(&len);
            continue;
        }

        // KILL стирает весь введённый текст
        if (ch == oldt.c_cc[VKILL]) {
            while (len > 0)
                erase_last(&len);
            continue;
        }

        // CTRL-W стирает последнее слово и пробелы после него
        if (ch == CTRL_W) {
            while (len > 0 && line[len - 1] == ' ')
                erase_last(&len);

            while (len > 0 && line[len - 1] != ' ')
                erase_last(&len);

            continue;
        }

        // CTRL-D завершает программу в начале ввода
        if (ch == CTRL_D) {
            if (len == 0)
                break;
            continue;
        }

        // Остальные непечатаемые символы вызывают звуковой сигнал
        if (!isprint(ch)) {
            write(STDOUT_FILENO, "\a", 1);
            continue;
        }

        // Увеличиваем буфер при необходимости
        if (len == capacity) {
            char *tmp = realloc(line, capacity * 2);

            if (!tmp) {
                perror("realloc");
                break;
            }

            line = tmp;
            capacity *= 2;
        }

        // Переносим оставшуюся часть длинного слова
        if (len > 0 && len % MAX_LINE == 0)
            write(STDOUT_FILENO, "\r\n", 2);

        // Сохраняем и сразу отображаем символ
        line[len++] = ch;
        write(STDOUT_FILENO, &ch, 1);
    }

    // Восстанавливаем настройки терминала
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    write(STDOUT_FILENO, "\n", 1);
    free(line);

    return 0;
}