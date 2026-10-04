#include <stdio.h>
#include <unistd.h>


void check_file(const char *filename)
{
    FILE *file;
    file = fopen(filename, "r");

    if (file == NULL)
    {
        perror("fopen");
    }
    else
    {
        printf("Файл открыт успешно\n");
        fclose(file);
    }
}


int main(void)
{
    printf("До setuid:\n");

    printf("UID = %ld, EUID = %ld\n",
           (long)getuid(),
           (long)geteuid());

    check_file("mibombo.txt");


    if (setuid(getuid()) == -1)
    {
        perror("setuid");
        return 1;
    }

    printf("\nПосле setuid:\n");

    printf("UID = %ld, EUID = %ld\n",
           (long)getuid(),
           (long)geteuid());

    check_file("mibombo.txt");

    return 0;
}
