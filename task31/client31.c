
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/task31"

int main() {
    int sock_fd;
    struct sockaddr_un addr;
    char buf[256];

    signal(SIGPIPE, SIG_IGN);

    // Создаём сокет
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    // Подключаемся к серверу
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Введите текст (Ctrl+D для завершения):\n");

    // Читаем текст и отправляем серверу
    while (1) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            perror("read");
            break;
        }
        if (n == 0)
            break;

        ssize_t sent = 0;

        while (sent < n) {
            ssize_t k = write(sock_fd, buf + sent, n - sent);

            if (k < 0 && errno == EINTR)
                continue;
            if (k <= 0) {
                perror("write");
                goto end;
            }

            sent += k;
        }
    }

end:
    shutdown(sock_fd, SHUT_WR);
    close(sock_fd);

    return 0;
}