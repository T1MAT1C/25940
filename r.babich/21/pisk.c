#include <unistd.h>
#include <signal.h>
#include <stdio.h>

int pisks = 0;

void pisk(int sig)
{
    printf("%c", (char)7);

    fflush(stdout);

    pisks++;
}

void ender(int sig)
{
    printf("Писков было - %d\n", pisks);

    _exit(0);
}

int main(void)
{
    signal(SIGINT, pisk);
    signal(SIGQUIT, ender);

    while(1)
    {

    }

    return 0;
}
