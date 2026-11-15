#include <ctype.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <err.h>
#include <stdlib.h>
#include <poll.h>
#include <stdio.h>

#define SOCKET_PATH "build/socket.sock"
#define SOCKET_CONN_LIMIT 32

#define print(msg) write(STDOUT_FILENO, msg, sizeof msg)

int main()
{
  unlink(SOCKET_PATH);

  print("SERVER: INITIALIZING...");

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);

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

  // poll
  int ready;
  struct pollfd pfds[SOCKET_CONN_LIMIT] = {
    { .fd=fd, .events=POLLIN }
  };
  int nfds = 1;
  print("\nSERVER: POLLING...");
  while (1)
  {
    // polling client fds
    ready = poll(pfds, nfds, -1);
    if (ready < 0) err(EXIT_FAILURE, "poll");

    // checking server fd
    if (pfds[0].revents != 0)
    {
      if (pfds[0].revents & POLLIN)
      {
        if (nfds >= SOCKET_CONN_LIMIT) continue;

        pfds[nfds] = (struct pollfd){ .events=POLLIN };
        // accept
        pfds[nfds].fd = accept(fd, NULL, NULL);
        if (pfds[nfds].fd < 0)
          err(EXIT_FAILURE, "accept");

        nfds++;

        print("\nSERVER: ACCEPTED NEW CONNECTION\n");
      }
      else
      {
        if (close(fd) < 0)
          err(EXIT_FAILURE, "close");
        print("\nSERVER: SHUTTING DOWN");
        break;
      }
    }
    // checking client fds
    for (int i = 1; i < nfds; ++i)
    {
      if (pfds[i].revents != 0)
      {
        if (pfds[i].revents & POLLIN)
        {
          char buf[256] = {0};
          int n = recv(pfds[i].fd, buf, sizeof(buf), 0);
          if (n < 0)
            err(EXIT_FAILURE, "recv");
          else if (n == 0) {
            print("\nSERVER: CLIENT DISCONNECTED\n");
            pfds[i] = pfds[nfds - 1];
            nfds--;
            i--;
          }
          for (int j = 0; j < strlen(buf); ++j)
            buf[j] = toupper(buf[j]);
          write(STDOUT_FILENO, buf, n);
        }
        else
        {
          if (close(pfds[i].fd) < 0)
            err(EXIT_FAILURE, "close");
          print("\nSERVER: CLIENT DISCONNECTED\n");
        }
      }
    }
  }

  unlink(SOCKET_PATH);
}