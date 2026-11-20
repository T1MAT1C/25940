#include <complex.h>
#include <ctype.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <aio.h>
#include <errno.h>
#include <string.h>
#ifdef __sun
#include <stropts.h>
#endif

#define SOCKET_PATH "build/socket.sock"
#define SOCKET_CONN_LIMIT 32

#define print(msg) write(STDOUT_FILENO, msg, sizeof msg)

int fd;
int clients[SOCKET_CONN_LIMIT] = {0};
int nclients = 0;

struct aiocb cb[SOCKET_CONN_LIMIT];
char abuf[SOCKET_CONN_LIMIT][256];

int arm(int i)
{
  memset(&cb[i], 0, sizeof cb[i]);
  cb[i].aio_fildes = clients[i];
  cb[i].aio_buf    = abuf[i];
  cb[i].aio_nbytes = sizeof abuf[i];
  cb[i].aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  cb[i].aio_sigevent.sigev_signo  = SIGIO;
  return aio_read(&cb[i]);
}

void set_async(int fd)
{
  fcntl(fd, F_SETOWN, getpid());
  int fl = fcntl(fd, F_GETFL);
#if defined(__sun)
  int sig_mask = S_INPUT | S_RDNORM;
  ioctl(fd, I_SETSIG, sig_mask);
  fcntl(fd, F_SETFL, fl | O_NONBLOCK);
#elif defined(__linux__)
  fcntl(fd, F_SETFL, fl | O_ASYNC | O_NONBLOCK);
#endif
}

void on_server_sigio(int sig)
{
  if (sig != SIGIO) return;

  // accept
  int client;
  while((client = accept(fd, NULL, NULL)) >= 0)
  {
    print("SERVER: NEW CONNECTION\n");

    if (nclients >= SOCKET_CONN_LIMIT) {
      close(client);
      continue;
    }

    clients[nclients] = client;
    if (arm(nclients) < 0) { close(client); continue; }
    nclients++;
  }

  // serve clients (aio)
  for (int i = 0; i < nclients; ++i)
  {
    int err = aio_error(&cb[i]);
    if (err == EINPROGRESS) continue;

    ssize_t n = (err == 0) ? aio_return(&cb[i]) : -1;

    if (n > 0) {
      for (ssize_t j = 0; j < n; ++j)
        abuf[i][j] = toupper((unsigned char)abuf[i][j]);
      write(STDOUT_FILENO, abuf[i], n);
      if (arm(i) == 0) continue;
      // fallthrough: перевооружить не удалось → закрываем
    }

    // n <= 0 (EOF/ошибка) или arm провалился → закрываем
    close(clients[i]);
    clients[i] = clients[--nclients];
    cb[i]       = cb[nclients];
    memcpy(abuf[i], abuf[nclients], sizeof abuf[i]);
    i--;
  }
  print("\n");
}

int main()
{
  print("SERVER: INITIALIZING...\n");

  unlink(SOCKET_PATH);

  fd = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un sa_server = {
    .sun_family = AF_UNIX,
    .sun_path = SOCKET_PATH
  };
  if (bind(fd, (struct sockaddr*)&sa_server, sizeof sa_server) < 0)
    err(EXIT_FAILURE, "bind");

  if (listen(fd, SOCKET_CONN_LIMIT) < 0)
    err(EXIT_FAILURE, "listen");

  set_async(fd);

#if defined(__sun)
  sigset(SIGIO, on_server_sigio);
#elif defined(__linux__)
  signal(SIGIO, on_server_sigio);
#endif

  print("SERVER: WAITING FOR CONNECTIONS...\n");
  while(1) pause();
}