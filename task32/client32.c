
#define __EXTENSIONS__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/task32"

// Отправляем все данные
int write_all(int fd, char *buf, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);

        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;

        sent += n;
    }

    return 0;
}

int main(int argc, char **argv) {
    int sock, count = 1, delay = 0;
    struct sockaddr_un addr;
    struct timespec ts;
    char msg[256];

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Использование: %s NAME [count] [delay_ms]\n",
                argv[0]);
        return 1;
    }

    if (argc > 2)
        count = atoi(argv[2]);
    if (argc > 3)
        delay = atoi(argv[3]);

    if (count < 1 || count > 10000 || delay < 0 || delay > 60000) {
        fprintf(stderr, "Неверное количество или задержка\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    // Создаём сокет
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    // Подключаемся к серверу
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock);
        return 1;
    }

    // Отправляем сообщения с именем клиента
    for (int i = 1; i <= count; i++) {
        int len = snprintf(msg, sizeof(msg), "%s#%d/%d\n",
                           argv[1], i, count);

        if (len < 0 || len >= (int)sizeof(msg)) {
            fprintf(stderr, "Слишком длинное имя\n");
            break;
        }

        if (write_all(sock, msg, (size_t)len) == -1) {
            perror("write");
            break;
        }

        if (delay > 0) {
            ts.tv_sec = delay / 1000;
            ts.tv_nsec = (long)(delay % 1000) * 1000000L;
            while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
                ;
        }
    }

    shutdown(sock, SHUT_WR);
    close(sock);

    return 0;
}