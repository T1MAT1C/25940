
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>

#define SOCKET_PATH "/tmp/task30"

int main(void) {
    int server_fd, client_fd;
    int esc_state = 0;
    struct sockaddr_un addr;
    char buf[1024];
    ssize_t n;

    // Удаление старого файла сокета
    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT) {
        perror("unlink");
        return 1;
    }

    // Создаём носок
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    // Привязываем сокет и ожидаем подключения клиента
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(server_fd, 1) == -1) {
        perror("bind/listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Сервер ожидает клиента...\n");

    // подключение клиента
    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    // текст от клиента
    while ((n = read(client_fd, buf, sizeof(buf))) != 0) {
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            perror("read");
            break;
        }

        int out = 0;

        // Игнорируем стрелки и другие Escape-последовательности
        for (ssize_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)buf[i];

            if (esc_state == 1) {
                if (c == '[') esc_state = 2;
                else if (c == 'O') esc_state = 3;
                else if (c != 27) {
                    esc_state = 0;
                    buf[out++] = (char)toupper(c);
                }
                continue;
            }

            if (esc_state == 2) {
                if (c == 27)
                    esc_state = 1;
                else if (c >= 0x40 && c <= 0x7E)
                    esc_state = 0;
                continue;
            }

            if (esc_state == 3) {
                esc_state = 0;
                continue;
            }

            if (c == 27) {
                esc_state = 1;
                continue;
            }

            buf[out++] = (char)toupper(c);
        }

        // обработанный текст
        if (out > 0) {
            fwrite(buf, 1, out, stdout);
            fflush(stdout);
        }
    }

    printf("\nКлиент отключился.\n");

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}