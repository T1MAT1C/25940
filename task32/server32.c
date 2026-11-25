
#define __EXTENSIONS__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/filio.h>

#define SOCKET_PATH "/tmp/task32"
#define MAX_CLIENTS 10
#define LINE_SIZE 512

struct Client {
    int fd;
    int esc;
    char line[LINE_SIZE];
    int len;
};

static struct Client clients[MAX_CLIENTS];
static volatile sig_atomic_t io_pending = 0;
static volatile sig_atomic_t stop = 0;
static int sync_mode = 0;

static struct timespec start_time;
static long sig_count = 0, poll_ready = 0, poll_idle = 0;
static long n_conn = 0, n_msg = 0, n_closed = 0;

// Обработчик SIGPOLL
void on_io(int sig) {
    (void)sig;
    io_pending = 1;
}

// Обработчик завершения
void on_stop(int sig) {
    (void)sig;
    stop = 1;
}

// Выводим время с момента запуска
void stamp(void) {
    struct timespec t;
    long sec, ns;

    clock_gettime(CLOCK_MONOTONIC, &t);
    sec = t.tv_sec - start_time.tv_sec;
    ns = t.tv_nsec - start_time.tv_nsec;

    if (ns < 0) {
        sec--;
        ns += 1000000000L;
    }

    printf("t=%ld.%02ld ", sec, ns / 10000000L);
}

// Включаем неблокирующий и асинхронный режим
int arm(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    int on = 1;

    if (flags == -1 ||
        fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;

    if (sync_mode)
        return 0;

    if (fcntl(fd, F_SETOWN, getpid()) == -1 ||
        ioctl(fd, FIOASYNC, &on) == -1 ||
        ioctl(fd, I_SETSIG,
              S_INPUT | S_MSG | S_HANGUP | S_ERROR) == -1)
        return -1;

    return 0;
}

// Выводим полученную строку
void print_line(int i) {
    clients[i].line[clients[i].len] = '\0';

    stamp();
    printf("fd=%d  %s\n", clients[i].fd, clients[i].line);
    fflush(stdout);

    clients[i].len = 0;
    n_msg++;
}

// Обрабатываем символ и игнорируем стрелочки
void process_char(int i, unsigned char c) {
    struct Client *cl = &clients[i];

    if (cl->esc == 1) {
        if (c == '[') cl->esc = 2;
        else if (c == 'O') cl->esc = 3;
        else if (c != 27) {
            cl->esc = 0;
            process_char(i, c);
        }
        return;
    }

    if (cl->esc == 2) {
        if (c == 27) cl->esc = 1;
        else if (c >= 0x40 && c <= 0x7E)
            cl->esc = 0;
        return;
    }

    if (cl->esc == 3) {
        cl->esc = 0;
        return;
    }

    if (c == 27) {
        cl->esc = 1;
        return;
    }

    c = (unsigned char)toupper(c);

    if (c == '\n') {
        print_line(i);
    } else if (cl->len < LINE_SIZE - 1) {
        cl->line[cl->len++] = c;
    } else {
        print_line(i);
        cl->line[cl->len++] = c;
    }
}

// Принимаем всех ожидающих клиентов
void accept_clients(int server_fd) {
    int c, i;

    while (1) {
        c = accept(server_fd, NULL, NULL);

        if (c < 0) {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                perror("accept");
            break;
        }

        for (i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd == -1)
                break;

        if (i == MAX_CLIENTS) {
            printf("Достигнут лимит клиентов\n");
            close(c);
            continue;
        }

        if (arm(c) == -1) {
            perror("arm client");
            close(c);
            continue;
        }

        clients[i].fd = c;
        clients[i].esc = 0;
        clients[i].len = 0;
        n_conn++;

        stamp();
        printf("Подключился клиент fd=%d\n", c);
    }
}

// Читаем данные от клиента
void read_client(int i) {
    char buf[512];
    ssize_t n;

    while (1) {
        n = read(clients[i].fd, buf, sizeof(buf));

        if (n < 0 && errno == EINTR)
            continue;

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        if (n <= 0) {
            int fd = clients[i].fd;

            if (clients[i].len > 0)
                print_line(i);

            close(fd);
            clients[i].fd = -1;
            clients[i].len = 0;
            clients[i].esc = 0;
            n_closed++;

            stamp();
            printf("Клиент fd=%d отключился\n", fd);
            break;
        }

        for (ssize_t j = 0; j < n; j++)
            process_char(i, (unsigned char)buf[j]);
    }
}

int main(int argc, char **argv) {
    int server_fd, r, nfds;
    struct sockaddr_un addr;
    struct sigaction sa;
    struct pollfd pf[MAX_CLIENTS + 1];

    sync_mode = (argc > 1 && strcmp(argv[1], "sync") == 0);

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Подготавливаем список клиентов
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;

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

    // Привязываем сокет и ожидаем подключения
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(server_fd, 16) == -1) {
        perror("bind/listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    // Настраиваем обработчики сигналов
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = on_io;
    if (sigaction(SIGPOLL, &sa, NULL) == -1) {
        perror("sigaction SIGPOLL");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    sa.sa_handler = on_stop;
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    // Настраиваем серверный сокет
    if (arm(server_fd) == -1) {
        perror("arm server");
        close(server_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Server %s | PID=%d | клиентов до %d\n",
           sync_mode ? "SYNC (poll 1 ms)" :
                       "ASYNC (SIGPOLL, poll fallback 250 ms)",
           (int)getpid(), MAX_CLIENTS);

    printf("Для завершения нажмите Ctrl+C.\n");

    while (!stop) {
        // Собираем список сокетов для poll
        nfds = 1;
        pf[0].fd = server_fd;
        pf[0].events = POLLIN;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                pf[nfds].fd = clients[i].fd;
                pf[nfds].events = POLLIN;
                nfds++;
            }
        }

        // Ждём событие или сигнал
        if (io_pending && !sync_mode) {
            r = -2;
        } else {
            r = poll(pf, nfds, sync_mode ? 1 : 250);
        }

        if (io_pending) {
            io_pending = 0;
            sig_count++;

            stamp();
            printf("Получен SIGPOLL: событие ввода-вывода\n");
        }

        if (r == -2) {
            // Уже получили SIGPOLL
        } else if (r < 0) {
            if (errno != EINTR) {
                perror("poll");
                break;
            }
        } else if (r == 0) {
            poll_idle++;
        } else {
            poll_ready++;
        }

        if (stop)
            break;

        // Проверяем новые подключения
        accept_clients(server_fd);

        // Проверяем данные всех клиентов
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd != -1)
                read_client(i);
    }

    // Выводим статистику
    printf("\nИТОГ РАБОТЫ:\n");
    printf("SIGPOLL: %ld\n", sig_count);
    printf("Событий poll: %ld\n", poll_ready);
    printf("Тайм-аутов poll: %ld\n", poll_idle);
    printf("Подключений: %ld\n", n_conn);
    printf("Сообщений: %ld\n", n_msg);
    printf("Отключений: %ld\n", n_closed);
    printf("Процессов: 1\n");

    // Закрываем все соединения
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1)
            close(clients[i].fd);

    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}