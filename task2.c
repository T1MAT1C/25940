#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

extern char *tzname[];
int main()
{
    time_t now = time(NULL); // время в секундах
    struct tm *sp;           // норм вид
    sp = gmtime(&now); // time_t в UTC
    printf("gmtime - %d/%d/%02d %d:%02d\n", sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min);

    sp = localtime(&now);
    printf("localtime - %d/%d/%02d %d:%02d %s\n", sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min, tzname[sp->tm_isdst]);

    time_t california = now - 15 * 3600;    // 15 часов от сервера

    sp = localtime(&california);

    if (sp->tm_mon > 2 && sp->tm_mon < 10) // всегда летнее время
    {
        california += 3600;
        sp = localtime(&california);
    }
    else if (sp->tm_mon == 2)   // март второе воскресенье в 2 часа ночи
    {
        int first_wday = (sp->tm_wday - (sp->tm_mday - 1) % 7 + 7) % 7; // номер дня недели первого числа месяца

        int first_sunday = 1 + (7 - first_wday) % 7; // от него считаем первое воскресенье

        if (sp->tm_mday > first_sunday + 7 || (sp->tm_mday == first_sunday + 7 && sp->tm_hour >= 2))
        {
            california += 3600;
            sp = localtime(&california);
        }
    }
    else if (sp->tm_mon == 10)  // ноябрь первое воскресенье в 2 часа ночи
    {
        int first_wday = (sp->tm_wday - (sp->tm_mday - 1) % 7 + 7) % 7;

        int first_sunday = 1 + (7 - first_wday) % 7;

        if (sp->tm_mday < first_sunday || (sp->tm_mday == first_sunday && sp->tm_hour < 1)) //
        {
            california += 3600;
            sp = localtime(&california);
        }
    }
    printf("california - %d/%d/%02d %d:%02d %s\n", sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min, tzname[sp->tm_isdst]);
    return 0;
}
