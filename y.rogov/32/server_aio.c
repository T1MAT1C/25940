#define _GNU_SOURCE
#include <aio.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef __sun
#include <stropts.h>
#endif

#define SOCKET_PATH        "build/socket.sock"
#define SOCKET_CONN_LIMIT  32
#define BUF_SZ             256

#define print(msg) write(STDOUT_FILENO, msg, sizeof msg)

static int listen_fd;

struct client_ctx {
    int          fd;
    int          in_use;
    struct aiocb cb;
    char         buf[BUF_SZ];
};

static struct client_ctx clients[SOCKET_CONN_LIMIT];

static void close_client(struct client_ctx *c);
static void arm_client  (struct client_ctx *c);

static void arm_client(struct client_ctx *c)
{
    memset(&c->cb, 0, sizeof c->cb);
    c->cb.aio_fildes = c->fd;
    c->cb.aio_buf    = c->buf;
    c->cb.aio_nbytes = sizeof c->buf;
    c->cb.aio_sigevent.sigev_notify          = SIGEV_SIGNAL;
    c->cb.aio_sigevent.sigev_signo           = SIGIO;
    c->cb.aio_sigevent.sigev_value.sival_ptr = c;   /* ← личность клиента */

    if (aio_read(&c->cb) < 0)
        close_client(c);
}

static void close_client(struct client_ctx *c)
{
    if (!c->in_use) return;
    if (aio_error(&c->cb) == EINPROGRESS)
        aio_cancel(c->fd, &c->cb);   /* на Linux может не сработать — см. ниже */
    close(c->fd);
    c->fd     = -1;
    c->in_use = 0;
}

static void do_accept(void)
{
    int client;
    while ((client = accept(listen_fd, NULL, NULL)) >= 0) {
        print("SERVER: NEW CONNECTION\n");

        int slot = -1;
        for (int i = 0; i < SOCKET_CONN_LIMIT; ++i)
            if (!clients[i].in_use) { slot = i; break; }
        if (slot < 0) { close(client); continue; }

        clients[slot].fd     = client;
        clients[slot].in_use = 1;
        arm_client(&clients[slot]);
    }
}

static void drain_completed(void)
{
    for (int i = 0; i < SOCKET_CONN_LIMIT; ++i) {
        struct client_ctx *c = &clients[i];
        if (!c->in_use) continue;

        int err = aio_error(&c->cb);
        if (err == EINPROGRESS) continue;

        if (err == 0) {
            ssize_t n = aio_return(&c->cb);
            if (n > 0) {
                for (ssize_t j = 0; j < n; ++j)
                    c->buf[j] = (char)toupper((unsigned char)c->buf[j]);
                write(STDOUT_FILENO, c->buf, (size_t)n);
                arm_client(c);
                printf("\n");
            } else {
                close_client(c);   /* 0 = EOF, клиент отвалился */
            }
        } else {
            close_client(c);
        }
    }
}

static void on_sigio(int sig, siginfo_t *si, void *uc)
{
    usleep(1000000);
    (void)uc;
    if (sig != SIGIO) return;

    if (si->si_value.sival_ptr != NULL) {
        /* aio completion — но сигналы схлопываются, поэтому проверяем всех */
        drain_completed();
    } else {
        /* ядро дёрнуло listen-fd (si_value для F_SETOWN/O_ASYNC = 0) */
        do_accept();
        drain_completed();
    }
}

static void set_async_listen(int fd)
{
    fcntl(fd, F_SETOWN, getpid());
    int fl = fcntl(fd, F_GETFL);
#if defined(__sun)
    ioctl(fd, I_SETSIG, S_INPUT | S_RDNORM);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#elif defined(__linux__)
    fcntl(fd, F_SETFL, fl | O_ASYNC | O_NONBLOCK);
#endif
}

int main(void)
{
    print("SERVER: INITIALIZING...\n");

    unlink(SOCKET_PATH);

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0) err(EXIT_FAILURE, "socket");

    struct sockaddr_un sa = {
        .sun_family = AF_UNIX,
        .sun_path   = SOCKET_PATH
    };
    if (bind(listen_fd, (struct sockaddr*)&sa, sizeof sa) < 0)
        err(EXIT_FAILURE, "bind");
    if (listen(listen_fd, SOCKET_CONN_LIMIT) < 0)
        err(EXIT_FAILURE, "listen");

    struct sigaction sg;
    memset(&sg, 0, sizeof sg);
    sg.sa_sigaction = on_sigio;
    sg.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sg.sa_mask);
    sigaction(SIGIO, &sg, NULL);

    set_async_listen(listen_fd);

    print("SERVER: WAITING FOR CONNECTIONS...\n");
    for (;;) pause();
}