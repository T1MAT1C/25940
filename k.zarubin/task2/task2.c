#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
extern char *tzname[];

int main(){
    printf("\n");
    time_t now;
    struct tm *sp;
    /* текущее время */
    (void) time(&now);
    printf("Local Time (BIOS):    %s", ctime(&now));

    /* ставим наше время на часовой пояс по Гринвичу (UTC+0) */
    sp = gmtime(&now);
    printf("UTC+0 Time:    %d/%d/%02d %02d:%02d UTC\n",
        sp->tm_mon + 1, sp->tm_mday,
        sp->tm_year + 1900, sp->tm_hour,
        sp->tm_min);

    /* меняем окружение на Pacific Standart Time */
    putenv("TZ=PST8");
    tzset();
    sp = localtime(&now);
    printf("PST8:    %d/%d/%02d %02d:%02d %s\n\n",
        sp->tm_mon + 1, sp->tm_mday,
        sp->tm_year + 1900, sp->tm_hour,
        sp->tm_min, tzname[sp->tm_isdst]);
    exit(0);
}
