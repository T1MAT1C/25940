
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE 40
#define CTRL_W 23
#define CTRL_D 4
#define CTRL_G 7

void print_char(char c) {
    putchar(c);
    fflush(stdout);
}

void erase_chars(int n) {
    while (n-- > 0)
        printf("\b \b");
    fflush(stdout);
}

int main() {
    struct termios oldt, newt;

    if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
        perror("tcgetattr");
        return 1;
    }

    newt = oldt;

    // Отключаем эхо, каноническую обработку и обработку сигналов
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {
        perror("tcsetattr");
        return 1;
    }

    char line[MAX_LINE];
    int len = 0;
    char ch;
    ssize_t r;

    while (1) {
        r = read(STDIN_FILENO, &ch, 1);

        if (r < 0 && errno == EINTR)
            continue;
        if (r != 1)
            break;

        // 2. При вводе ERASE стираем последний символ
        if ((unsigned char)ch == oldt.c_cc[VERASE]) {
            if (len > 0) {
                len--;
                erase_chars(1);
            }
            continue;
        }

        // 3. При вводе KILL стираем всю текущую строку
        if ((unsigned char)ch == oldt.c_cc[VKILL]) {
            erase_chars(len);
            len = 0;
            continue;
        }

        // 4. При CTRL-W стираем последнее слово и следующие за ним пробелы
        if (ch == CTRL_W) {
            while (len > 0 && line[len - 1] == ' ') {
                len--;
                erase_chars(1);
            }
            while (len > 0 && line[len - 1] != ' ') {
                len--;
                erase_chars(1);
            }
            continue;
        }

        // 5. Завершаем программу, если CTRL-D введён в начале строки
        if (ch == CTRL_D) {
            if (len == 0)
                break;
            continue;
        }

        // 6. Все остальные непечатаемые символы заменяем звуковым сигналом CTRL-G
        if (!isprint((unsigned char)ch)) {
            print_char(CTRL_G);
            continue;
        }

        // 7. Если слово пересекает 40-й столбец, переносим его в начало следующей строки
        if (len == MAX_LINE && ch != ' ') {
            int start = len;

            while (start > 0 && line[start - 1] != ' ')
                start--;

            if (start == 0) {
                print_char(CTRL_G);
                continue;
            }

            int word_len = len - start;
            int end = start;

            while (end > 0 && line[end - 1] == ' ')
                end--;

            erase_chars(len - end);
            print_char('\n');

            memmove(line, line + start, word_len);
            len = word_len;

            for (int i = 0; i < len; i++)
                print_char(line[i]);
        }
        else if (len == MAX_LINE && ch == ' ') {
            print_char('\n');
            len = 0;
        }

        // 1. Каждый введённый символ немедленно отображаем на экране
        line[len++] = ch;
        print_char(ch);
    }

    // Восстанавливаем настройки терминала
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldt) == -1)
        perror("tcsetattr");

    print_char('\n');
    return 0;
}