#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    time_t t;
    struct tm *tm;

    time(&t);
    tm = localtime(&t);
    printf("do: %s", asctime(tm));

    setenv("TZ", "America/Los_Angeles", 1);
    tzset();


    // setenv("TZ", "PST8", 1);
    // tzset();

    tm = localtime(&t);
    printf("[posle]:  %s", asctime(tm));

    return 0;
}
