
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
    struct sockaddr_un addr;
    char buf[1024];
    ssize_t n;

    // удаление старого файла сокета
    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT) {
        perror("unlink");
        return 1;
    }

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);    // носок
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    // Привязываем сокет к батарее и начинаем ожидать клиента
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(server_fd, 1) == -1) {
        perror("bind/listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Сервер ожидает клиента...\n");

    // Принимаем ислам
    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    // переводим в верхний регистр
    while ((n = read(client_fd, buf, sizeof(buf))) != 0) {
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            perror("read");
            break;
        }

        for (ssize_t i = 0; i < n; i++)
            buf[i] = (char)toupper((unsigned char)buf[i]);

        fwrite(buf, 1, n, stdout);
        fflush(stdout);
    }

    printf("\nКлиент отключился.\n");

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}