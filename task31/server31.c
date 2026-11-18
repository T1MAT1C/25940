
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SOCKET_PATH "/tmp/task31"

int main() {
    int server_fd, client_fd, max_fd;
    int esc_state = 0;
    fd_set master, readfds;
    struct sockaddr_un addr;
    char buf[1024];

    // Удаляем старый сокет
    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT) {
        perror("unlink");
        return 1;
    }

    // Создаём серверный сокет
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    // Привязываем сокет и ожидаем клиентов
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(server_fd, 10) == -1) {
        perror("bind/listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    FD_ZERO(&master);
    FD_SET(server_fd, &master);
    max_fd = server_fd;

    printf("Сервер ожидает клиентов...\n");

    while (1) {
        readfds = master;

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (!FD_ISSET(fd, &readfds))
                continue;

            // Принимаем ислам
            if (fd == server_fd) {
                client_fd = accept(server_fd, NULL, NULL);

                if (client_fd == -1)
                    continue;

                if (client_fd >= FD_SETSIZE) {
                    close(client_fd);
                    continue;
                }

                FD_SET(client_fd, &master);

                if (client_fd > max_fd)
                    max_fd = client_fd;

                printf("Подключился новый клиент\n");
            }
            else {
                ssize_t n = read(fd, buf, sizeof(buf));

                if (n < 0 && errno == EINTR)
                    continue;

                if (n <= 0) {
                    if (n < 0)
                        perror("read");

                    printf("Клиент отключился\n");
                    close(fd);
                    FD_CLR(fd, &master);
                    continue;
                }

                // Игнорируем стрелки и другие Escape-последовательности
                int out = 0;

                for (ssize_t i = 0; i < n; i++) {
                    unsigned char c = (unsigned char)buf[i];

                    if (esc_state == 0 && c == 27) {
                        esc_state = 1;
                        continue;
                    }

                    if (esc_state == 1) {
                        if (c == '[') esc_state = 2;
                        else if (c == 'O') esc_state = 3;
                        else {
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

                    buf[out++] = (char)toupper(c);
                }

                if (out > 0) {
                    fwrite(buf, 1, out, stdout);
                    fflush(stdout);
                }
            }
        }
    }

    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &master) && fd != server_fd)
            close(fd);
    }

    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}