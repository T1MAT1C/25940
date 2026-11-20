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
#include <errno.h>
#include <sys/file.h>
#ifdef __sun
#include <stropts.h>
#endif

#define SOCKET_PATH "build/socket.sock"
#define SOCKET_CONN_LIMIT 32

#define print(msg) write(STDOUT_FILENO, msg, sizeof msg)

int fd;
int clients[SOCKET_CONN_LIMIT] = {0};
int nclients = 0;

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
  // usleep(1000000);

  // accept
  int client;
  while((client = accept(fd, NULL, NULL)) >= 0)
  {
    print("SERVER: NEW CONNECTION\n");
    set_async(client);
    
    if (nclients >= SOCKET_CONN_LIMIT) {
      close(client);
      continue;
    }

    clients[nclients++] = client;
  }

  // serve clients
  char buf[256] = {0};
  for (int i = 0; i < nclients; ++i)
  {
    int n;
    while((n = recv(clients[i], buf, sizeof(buf), 0)) > 0)
    {
      for (int j = 0; j < n; ++j)
        buf[j] = toupper(buf[j]);
      write(STDOUT_FILENO, buf, n);
      print("\n");
    }

    if (n == -1 && errno == EAGAIN)
      continue;
    if (n <= 0)
    {
      close(clients[i]);
      clients[i] = clients[--nclients];
      i--;
    }
  }
}

int main()
{
  print("SERVER: INITIALIZING...\n");

  unlink(SOCKET_PATH);

  fd = socket(AF_UNIX, SOCK_STREAM, 0);

  // bind
  struct sockaddr_un sa_server = {
    .sun_family = AF_UNIX,
    .sun_path = SOCKET_PATH
  };
  if (bind(fd, (struct sockaddr*)&sa_server, sizeof sa_server) < 0)
    err(EXIT_FAILURE, "bind");

  // listen
  if (listen(fd, SOCKET_CONN_LIMIT) < 0)
    err(EXIT_FAILURE, "listen");

  // set server async
  set_async(fd);

#if defined(__sun)
  sigset(SIGIO, on_server_sigio);
#elif defined(__linux__)
  signal(SIGIO, on_server_sigio);
#endif

  print("SERVER: WAITING FOR CONNECTIONS...\n");
  while(1) pause();
}
