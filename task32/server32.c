
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/task32"
#define MAX_CLIENTS 100

struct Client {
    int fd;
    int esc;
};

struct Client clients[MAX_CLIENTS];
volatile sig_atomic_t sigio_flag = 0;

// Обработчик сигнала SIGIO
void handle_sigio(int sig) {
    (void)sig;
    sigio_flag = 1;
}

// Включаем асинхронный режим сокета
int set_async(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1 ||
        fcntl(fd, F_SETOWN, getpid()) == -1 ||
        fcntl(fd, F_SETFL, flags | FNDELAY | FASYNC) == -1)
        return -1;

    return 0;
}

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_un addr;
    struct sigaction sa = {0};
    sigset_t set, oldset;
    char buf[1024];

    // Настраиваем обработчик SIGIO
    sigemptyset(&set);
    sigaddset(&set, SIGIO);
    sigprocmask(SIG_BLOCK, &set, &oldset);

    sa.sa_handler = handle_sigio;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGIO, &sa, NULL);

    // Подготавливаем список клиентов
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;

    unlink(SOCKET_PATH);

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
        listen(server_fd, 10) == -1 || set_async(server_fd) == -1) {
        perror("bind/listen/async");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Сервер ожидает клиентов...\n");

    while (1) {
        // Ждём уведомления о готовности сокета
        while (!sigio_flag)
            sigsuspend(&oldset);

        sigio_flag = 0;

        // Принимаем всех ожидающих клиентов
        while ((client_fd = accept(server_fd, NULL, NULL)) >= 0) {
            int i;

            for (i = 0; i < MAX_CLIENTS; i++)
                if (clients[i].fd == -1)
                    break;

            if (i == MAX_CLIENTS || set_async(client_fd) == -1) {
                close(client_fd);
                continue;
            }

            clients[i].fd = client_fd;
            clients[i].esc = 0;
            printf("Подключился новый клиент\n");
        }

        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)
            perror("accept");

        // Получаем данные от всех клиентов
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == -1)
                continue;

            while (1) {
                ssize_t n = read(clients[i].fd, buf, sizeof(buf));

                if (n < 0 && errno == EINTR)
                    continue;

                if (n <= 0) {
                    if (n < 0 && errno != EWOULDBLOCK &&
                        errno != EAGAIN)
                        perror("read");

                    if (n == 0 || (n < 0 && errno != EWOULDBLOCK &&
                                   errno != EAGAIN)) {
                        printf("Клиент отключился\n");
                        close(clients[i].fd);
                        clients[i].fd = -1;
                    }
                    break;
                }

                // Игнорируем стрелочки и другие Escape-последовательности
                int out = 0;

                for (ssize_t j = 0; j < n; j++) {
                    unsigned char c = (unsigned char)buf[j];

                    if (clients[i].esc == 1) {
                        if (c == '[') clients[i].esc = 2;
                        else if (c == 'O') clients[i].esc = 3;
                        else if (c != 27) {
                            clients[i].esc = 0;
                            buf[out++] = (char)toupper(c);
                        }
                        continue;
                    }

                    if (clients[i].esc == 2) {
                        if (c == 27) clients[i].esc = 1;
                        else if (c >= 0x40 && c <= 0x7E)
                            clients[i].esc = 0;
                        continue;
                    }

                    if (clients[i].esc == 3) {
                        clients[i].esc = 0;
                        continue;
                    }

                    if (c == 27) {
                        clients[i].esc = 1;
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

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}