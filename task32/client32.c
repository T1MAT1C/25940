#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#define PATH "socket32"

int main() {
    int fd;
    char buf[1024];
    struct sockaddr_un addr = {0};

    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, PATH);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 1;
    }

    while (fgets(buf, sizeof(buf), stdin))
        write(fd, buf, strlen(buf));

    close(fd);
    return 0;
}