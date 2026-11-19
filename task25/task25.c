
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];
    pid_t pid;
    char buf[256];

    // Создаём программный канал
    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Дочерний процесс читает данные из канала
        close(fd[1]);

        int esc = 0;
        ssize_t n;

        while ((n = read(fd[0], buf, sizeof(buf))) != 0) {
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0) {
                perror("read");
                break;
            }

            int out = 0;

            // Игнорируем стрелочки и другие Escape-последовательности
            for (ssize_t i = 0; i < n; i++) {
                unsigned char c = (unsigned char)buf[i];

                if (esc == 1) {
                    if (c == '[') esc = 2;
                    else if (c == 'O') esc = 3;
                    else if (c != 27) {
                        esc = 0;
                        buf[out++] = (char)toupper(c);
                    }
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

                buf[out++] = (char)toupper(c);
            }

            if (out > 0)
                write(STDOUT_FILENO, buf, out);
        }

        close(fd[0]);
    }
    else {
        // Родительский процесс отправляет текст в канал
        close(fd[0]);

        printf("Введите текст (Ctrl+D для завершения):\n");

        ssize_t n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            ssize_t sent = 0;

            while (sent < n) {
                ssize_t k = write(fd[1], buf + sent, n - sent);

                if (k < 0 && errno == EINTR)
                    continue;
                if (k <= 0) {
                    perror("write");
                    break;
                }

                sent += k;
            }

            if (sent < n)
                break;
        }

        close(fd[1]);
        waitpid(pid, NULL, 0);
    }

    return 0;
}