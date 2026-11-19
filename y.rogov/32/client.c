#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>

#define SOCKET_PATH "./build/socket.sock"

#define print(msg) write(STDOUT_FILENO, msg, sizeof msg)

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    printf("USAGE: %s [msg]\n", argv[0]);
    return EXIT_SUCCESS;
  }
  int delay = atoi(argv[2]);

  print("CLIENT: INITIALIZING...\n");

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
  char *msg = argv[1];
  print("CLIENT: SENDING MESSAGE: '");
  write(STDOUT_FILENO, msg, strlen(msg));
  print("'\n");
  while (1)
  {
    if (send(sock_fd, msg, strlen(msg), 0) < 0)
    {
      perror("send");

      if (close(sock_fd) < 0)
      {
        perror("close");
        return EXIT_FAILURE;
      }
      return EXIT_FAILURE;
    }
    sleep(delay);
  }
}
