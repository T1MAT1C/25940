#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <ctype.h>
#include <string.h>

#define SOCKET_PATH "./25/socket.sock"

int main()
{
  // fork
  pid_t pid = fork();
  if (pid < 0)
  {
    perror("fork");
    return EXIT_FAILURE;
  }
  else if (pid == 0)
  { // CLIENT
    sleep(1); // wait for parent

    write(STDOUT_FILENO, "CHILD: HELLO FROM CHILD!\n", 25);

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
    char msg[] = "hello from child!!!";
    if (send(sock_fd, msg, strlen(msg), 0) < 0)
    {
      perror("send");
      return EXIT_FAILURE;
    }

    write(STDOUT_FILENO, "CHILD: I'VE SENT A MESSAGE:\n", 28);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\n", 1);

    close(sock_fd);
  }
  else 
  { // SERVER
    write(STDOUT_FILENO, "PARENT: HELLO FROM PARENT!\n", 27);

    // socket
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
      perror("socket");
      return EXIT_FAILURE;
    }

    // bind addr
    struct sockaddr_un server_addr = { 
      .sun_family = AF_UNIX,
      .sun_path = SOCKET_PATH
    };
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
      perror("bind");
      return EXIT_FAILURE;
    }

    // listen client
    const int conn_limit = 1;
    if (listen(sock_fd, conn_limit) < 0)
    {
      perror("listen");
      return EXIT_FAILURE;
    }
    
    // accept client
    int client_fd = accept(sock_fd, NULL, NULL);
    if (client_fd < 0)
    {
      perror("accept");
      return EXIT_FAILURE;
    }

    // recv
    char msg[256];
    ssize_t size;
    while ((size = recv(client_fd, msg, sizeof(msg), 0)) > 0)
    {
      for (ssize_t i = 0; i < size; ++i) msg[i] = toupper(msg[i]);

      // output msg
      write(STDOUT_FILENO, "PARENT: OMG!!! I'VE GOT A MESSAGE:\n", 35);
      write(STDOUT_FILENO, msg, size);
      write(STDOUT_FILENO, "\n", 1);
    }
    if (size < 0)
    {
      perror("recv");
      return EXIT_FAILURE;
    }

    wait(0);
    close(sock_fd);
    close(client_fd);

    unlink(SOCKET_PATH);
  }
}
