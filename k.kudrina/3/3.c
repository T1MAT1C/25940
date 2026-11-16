#include <stdio.h>
#include <unistd.h>

int main(void){
    int uid = getuid();
    int euid = geteuid();
    printf("> RUID: %d\n", uid);
    printf("  EUID: %d\n", euid);

    FILE* f = fopen("secret.txt","r");
    if (f != NULL) fclose(f);
    else perror("file not opened");

    setuid(euid);
    uid = getuid();
    printf("> RUID: %d\n", uid);
    printf("  EUID: %d\n", euid);

    FILE* m = fopen("secret.txt","r");
    if (m != NULL) fclose(m);
    else perror("file not opened");
}
