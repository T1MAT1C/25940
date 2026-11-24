
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 40
#define ERASE 127
#define KILL 21
#define CTRL_W 23
#define CTRL_D 4
#define CTRL_G 7

// Выводим символы на экран
void print_bytes(const char *s, size_t len) {
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, s, len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        s += n;
        len -= n;
    }
}

void print_char(char c) {
    print_bytes(&c, 1);
}

// Удаляем последний символ
void erase_last(char *line, size_t *len, int *col) {
    if (*len == 0)
        return;

    (*len)--;
    line[*len] = '\0';

    // Если курсор в начале строки, переходим на предыдущую
    if (*col == 0) {
        print_bytes("\033[1A\033[40G\033[X", 12);
        *col = MAX_LINE - 1;
    } else {
        print_bytes("\b \b", 3);
        (*col)--;
    }
}

int main(void) {
    struct termios oldt, newt;

    if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
        perror("tcgetattr");
        return 1;
    }

    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {
        perror("tcsetattr");
        return 1;
    }

    size_t capacity = 128;
    size_t len = 0;
    int col = 0;
    int esc = 0;

    char *line = malloc(capacity);
    if (!line) {
        perror("malloc");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return 1;
    }

    char ch;
    ssize_t n;

    while ((n = read(STDIN_FILENO, &ch, 1)) == 1) {
        unsigned char c = (unsigned char)ch;

        // Игнорируем стрелочки
        if (esc == 1) {
            if (c == '[') esc = 2;
            else if (c == 'O') esc = 3;
            else if (c != 27) esc = 0;
            continue;
        }

        if (esc == 2) {
            if (c >= 0x40 && c <= 0x7E)
                esc = 0;
            continue;
        }

        if (esc == 3) {
            esc = 0;
            continue;
        }

        if (c == 27) {
            esc = 1;
            continue;
        }

        // CTRL-D завершает программу в начале строки
        if (c == CTRL_D) {
            if (len == 0)
                break;
            continue;
        }

        // ERASE удаляет последний символ
        if (c == ERASE || c == 8) {
            erase_last(line, &len, &col);
            continue;
        }

        // KILL удаляет всю введённую строку
        if (c == KILL) {
            while (len > 0)
                erase_last(line, &len, &col);
            continue;
        }

        // CTRL-W удаляет последнее слово и пробелы после него
        if (c == CTRL_W) {
            while (len > 0 && line[len - 1] == ' ')
                erase_last(line, &len, &col);

            while (len > 0 && line[len - 1] != ' ')
                erase_last(line, &len, &col);

            continue;
        }

        // Непечатаемые символы издают звуковой сигнал
        if (!isprint(c)) {
            print_char(CTRL_G);
            continue;
        }

        // Увеличиваем буфер при необходимости
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *tmp = realloc(line, capacity);

            if (!tmp) {
                perror("realloc");
                break;
            }

            line = tmp;
        }

        // Сохраняем и выводим символ
        line[len++] = ch;
        line[len] = '\0';
        print_char(ch);

        col++;

        // После 40 символов переносим оставшийся ввод на новую строку
        if (col == MAX_LINE) {
            print_char('\n');
            col = 0;
        }
    }

    if (n < 0 && errno != EINTR)
        perror("read");

    // Восстанавливаем настройки терминала
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    print_char('\n');
    free(line);

    return 0;
}