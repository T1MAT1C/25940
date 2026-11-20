#define __EXTENSIONS__
#define _POSIX_C_SOURCE 200809L
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

#define SOCKET_PATH       "build/socket.sock"
#define SOCKET_CONN_LIMIT 32
#define BUF_SZ            256

struct client {
    int fd;
    int in_use;
    struct aiocb cb;
    char buf[BUF_SZ];
};

static int listen_fd;
static struct client clients[SOCKET_CONN_LIMIT];
static sigset_t wait_set;

/* Solaris queues this tagged completion signal because SA_SIGINFO is set. */
static void mark_signals_queued(int sig, siginfo_t *info, void *context)
{
    (void)sig;
    (void)info;
    (void)context;
}

static void close_client(struct client *c)
{
    if (!c->in_use)
        return;

    if (aio_error(&c->cb) == EINPROGRESS)
        (void)aio_cancel(c->fd, &c->cb);
    (void)close(c->fd);
    c->fd = -1;
    c->in_use = 0;
}

static void start_read(struct client *c)
{
    memset(&c->cb, 0, sizeof c->cb);
    c->cb.aio_fildes = c->fd;
    c->cb.aio_buf = c->buf;
    c->cb.aio_nbytes = sizeof c->buf;
    c->cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    c->cb.aio_sigevent.sigev_signo = SIGRTMIN;
    c->cb.aio_sigevent.sigev_value.sival_ptr = c;

    if (aio_read(&c->cb) < 0)
        close_client(c);
}

static void accept_clients(void)
{
    for (;;) {
        int fd = accept(listen_fd, NULL, NULL);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            if (errno == EINTR)
                continue;
            err(EXIT_FAILURE, "accept");
        }

        struct client *c = NULL;
        for (int i = 0; i < SOCKET_CONN_LIMIT; ++i) {
            if (!clients[i].in_use) {
                c = &clients[i];
                break;
            }
        }
        if (c == NULL) {
            close(fd);
            continue;
        }

        c->fd = fd;
        c->in_use = 1;
        start_read(c);
        (void)write(STDOUT_FILENO, "SERVER: NEW CONNECTION\n",
                    sizeof "SERVER: NEW CONNECTION\n" - 1);
    }
}

static void complete_client(struct client *c)
{
    if (c == NULL || !c->in_use)
        return;

    int error = aio_error(&c->cb);
    if (error == EINPROGRESS)
        return;
    if (error != 0) {
        close_client(c);
        return;
    }

    ssize_t n = aio_return(&c->cb);
    if (n <= 0) {
        close_client(c);
        return;
    }

    for (ssize_t i = 0; i < n; ++i)
        c->buf[i] = (char)toupper((unsigned char)c->buf[i]);
    (void)write(STDOUT_FILENO, c->buf, (size_t)n);
    (void)write(STDOUT_FILENO, "\n", 1);
    start_read(c);
}

int main(void)
{
    (void)write(STDOUT_FILENO, "SERVER: INITIALIZING...\n",
                sizeof "SERVER: INITIALIZING...\n" - 1);
    (void)unlink(SOCKET_PATH);

    sigemptyset(&wait_set);
    sigaddset(&wait_set, SIGIO);
    sigaddset(&wait_set, SIGRTMIN);
    if (sigprocmask(SIG_BLOCK, &wait_set, NULL) < 0)
        err(EXIT_FAILURE, "sigprocmask");

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0)
        err(EXIT_FAILURE, "socket");

    struct sockaddr_un address = {
        .sun_family = AF_UNIX,
        .sun_path = SOCKET_PATH
    };
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof address) < 0)
        err(EXIT_FAILURE, "bind");
    if (listen(listen_fd, SOCKET_CONN_LIMIT) < 0)
        err(EXIT_FAILURE, "listen");

    int flags = fcntl(listen_fd, F_GETFL);
    if (flags < 0 || fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        err(EXIT_FAILURE, "fcntl O_NONBLOCK");
    if (fcntl(listen_fd, F_SETOWN, getpid()) < 0)
        err(EXIT_FAILURE, "fcntl F_SETOWN");
#ifdef __sun
    if (ioctl(listen_fd, I_SETSIG, S_INPUT | S_RDNORM) < 0)
        err(EXIT_FAILURE, "I_SETSIG");
#else
    if (fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK | O_ASYNC) < 0)
        err(EXIT_FAILURE, "fcntl O_ASYNC");
#endif

    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_sigaction = mark_signals_queued;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGRTMIN, &action, NULL) < 0)
        err(EXIT_FAILURE, "sigaction");

    (void)write(STDOUT_FILENO, "SERVER: WAITING FOR CONNECTIONS...\n",
                sizeof "SERVER: WAITING FOR CONNECTIONS...\n" - 1);
    for (;;) {
        siginfo_t info;
        int sig = sigwaitinfo(&wait_set, &info);
        if (sig < 0) {
            if (errno == EINTR)
                continue;
            err(EXIT_FAILURE, "sigwaitinfo");
        }

        if (sig == SIGIO) {
            accept_clients();
        } else if (sig == SIGRTMIN) {
            complete_client((struct client *)info.si_value.sival_ptr);
        }
    }
}
