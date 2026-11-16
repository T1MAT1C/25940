#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void)
{
    uid_t ruid, euid;
    FILE *file;

    printf("First try:\n");
    printf("real: %d\n", getuid());
    printf("effect UID: %d\n", geteuid());

    file = fopen("zh.txt", "r");

    if (file == NULL) {
        perror("fopen");
    } else {
        printf("File opened successfully\n");
        fclose(file);
    }

    setuid(getuid());

    printf("\nSecond try:\n");
    printf("real: %d\n", getuid());
    printf("effect : %d\n", geteuid());

    file = fopen("zh.txt", "r");

    if (file == NULL) {
        perror("fopen");
    } else {
        printf("File opened successfully\n");
        fclose(file);
    }

    return 0;
}
