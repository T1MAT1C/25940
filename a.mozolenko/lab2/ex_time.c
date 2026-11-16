#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main(void)
{
    time_t now;
    struct tm *pacific_time;
    char *time_string;
    char zone[16];

    if (setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1) == -1) {
        perror("setenv");
        return EXIT_FAILURE;
    }
    tzset();

    now = time(NULL);
    if (now == (time_t)-1) {
        perror("time");
        return EXIT_FAILURE;
    }

    time_string = ctime(&now);
    if (time_string == NULL) {
        perror("ctime");
        return EXIT_FAILURE;
    }
    fputs(time_string, stdout);

    pacific_time = localtime(&now);
    if (pacific_time == NULL) {
        perror("localtime");
        return EXIT_FAILURE;
    }

    if (strftime(zone, sizeof(zone), "%Z", pacific_time) == 0) {
        fprintf(stderr, "strftime: unable to format time zone\n");
        return EXIT_FAILURE;
    }

    printf("%d/%d/%04d %d:%02d %s\n",
        pacific_time->tm_mon + 1,
        pacific_time->tm_mday,
        pacific_time->tm_year + 1900,
        pacific_time->tm_hour,
        pacific_time->tm_min,
        zone);

    return EXIT_SUCCESS;
}
