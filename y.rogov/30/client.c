#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <string.h>

#define SOCKET_PATH "./build/socket.sock"

int main()
{
  write(STDOUT_FILENO, "CLIENT: HELLO FROM CLIENT!\n", 27);

  // socket
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock_fd < 0)
  {
    perror("socket");
    return EXIT_FAILURE;
  }

  //connect
  struct sockaddr_un server_addr = { 
    .sun_family = AF_UNIX,
    .sun_path = SOCKET_PATH
  };
  if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("connect");
    return EXIT_FAILURE;
  }

  // send
  char msg[] = "Hello from Client!";
  if (send(sock_fd, msg, strlen(msg), 0) < 0)
  {
    perror("send");
    return EXIT_FAILURE;
  }

  write(STDOUT_FILENO, "CLIENT: I'VE SENT A MESSAGE:\n", 29);
  write(STDOUT_FILENO, msg, strlen(msg));
  write(STDOUT_FILENO, "\n", 1);

  close(sock_fd);
}
