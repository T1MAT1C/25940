#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(){
    system("chmod 664 file.txt");
    
    int fd = open("file.txt", O_RDWR);
    if (fd == -1){
        perror("open");
        return 1;
    }
    
    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLKW, &lock) == -1){
        perror("fcntl");
        close(fd);
        return 1;
    }
    printf("File locked. Opening editor(ed):\n");
    fflush(stdout);
    
    int status = system("ed file.txt");
    if (status == -1){
        perror("system");
    }
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) == -1){
        perror("unlock");
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}
