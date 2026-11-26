#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#define PATH "socket32"
#define MAX 10

void async(int fd) {
    if (fcntl(fd, F_SETOWN, getpid()) == -1 ||
        fcntl(fd, F_SETFL, O_NONBLOCK | FASYNC) == -1) {
        perror("fcntl");
        exit(1);
    }
}

int main() {
    int server, fd, n, i, j, sig;
    int clients[MAX] = {0};
    char buf[1024];
    struct sockaddr_un addr = {0};
    sigset_t signals;

    sigemptyset(&signals);
    sigaddset(&signals, SIGIO);
    sigprocmask(SIG_BLOCK, &signals, NULL);

    server = socket(AF_UNIX, SOCK_STREAM, 0);

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, PATH);

    unlink(PATH);
    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, MAX);

    async(server);

    printf("Server started\n");

    while (1) {
        while ((fd = accept(server, NULL, NULL)) >= 0) {
            for (i = 0; i < MAX && clients[i]; i++);

            if (i == MAX) {
                close(fd);
                continue;
            }

            clients[i] = fd;
            async(fd);
            printf("Client %d connected\n", fd);
        }

        for (i = 0; i < MAX; i++) {
            if (!clients[i])
                continue;

            while ((n = read(clients[i], buf, sizeof(buf))) > 0) {
                for (j = 0; j < n; j++)
                    buf[j] = toupper((unsigned char)buf[j]);

                write(1, buf, n);
            }

            if (n == 0) {
                printf("Client %d disconnected\n", clients[i]);
                close(clients[i]);
                clients[i] = 0;
            }
        }

        sigwait(&signals, &sig);
    }
}