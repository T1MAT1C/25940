
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 40
#define CTRL_W 23
#define CTRL_D 4
#define CTRL_G 7

// Стираем символы с экрана
void erase(int n) {
    while (n-- > 0)
        printf("\b \b");
    fflush(stdout);
}

int main(void) {
    struct termios oldt, newt;

    // Получаем настройки терминала
    if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
        perror("tcgetattr");
        return 1;
    }

    newt = oldt;

    // Выключаем эхо и каноническую обработку
    newt.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    newt.c_iflag &= ~IXON;
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {
        perror("tcsetattr");
        return 1;
    }

    char line[MAX_LINE];
    unsigned char c;
    int len = 0, esc = 0;
    ssize_t n;

    while (1) {
        n = read(STDIN_FILENO, &c, 1);

        if (n < 0 && errno == EINTR)
            continue;
        if (n != 1)
            break;

        // Игнорируем управляющие последовательности стрелок
        if (c == 27) {
            esc = 1;
            putchar(CTRL_G);
            fflush(stdout);
            continue;
        }

        if (esc == 1) {
            esc = 0;
            if (c == '[' || c == 'O') {
                esc = 2;
                continue;
            }
        }

        if (esc == 2) {
            if (c >= 0x40 && c <= 0x7E)
                esc = 0;
            continue;
        }

        // 2. ERASE стирает последний символ текущей строки
        if (c == oldt.c_cc[VERASE]) {
            if (len > 0) {
                len--;
                erase(1);
            }
        }

        // 3. KILL стирает все символы текущей строки
        else if (c == oldt.c_cc[VKILL]) {
            erase(len);
            len = 0;
        }

        // 4. CTRL-W стирает последнее слово и следующие за ним пробелы
        else if (c == CTRL_W) {
            while (len > 0 && line[len - 1] == ' ') {
                len--;
                erase(1);
            }

            while (len > 0 && line[len - 1] != ' ') {
                len--;
                erase(1);
            }
        }

        // 5. CTRL-D завершает программу в начале строки
        else if (c == CTRL_D) {
            if (len == 0)
                break;
        }

        // 6. Непечатаемые символы издают звуковой сигнал
        else if (!isprint(c)) {
            putchar(CTRL_G);
            fflush(stdout);
        }

        else {
            // 7. После 40 символов переходим на новую строку
            if (len == MAX_LINE) {
                putchar('\n');
                len = 0;
            }

            // 1. Каждый введённый символ появляется немедленно
            line[len++] = c;
            putchar(c);
            fflush(stdout);
        }
    }

    // Восстанавливаем настройки терминала
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    putchar('\n');

    return 0;
}