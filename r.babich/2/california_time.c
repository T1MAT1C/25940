#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

extern char *tzname[];
 
//gcc -Wall -Wextra -Wpedantic -g california_time.c -o res
int main()
{
    /* Фитовская херь
    time_t now;
    struct tm *sp;

    (void) time( &now );

    printf("global - %s", ctime( &now ) );

    sp = localtime(&now);

    printf("local - %d/%d/%02d %d:%02d %s\n", sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min, tzname[sp->tm_isdst]);

    time_t tm = mktime(sp);

    tm -= 50400;

    sp = localtime(&tm);

    printf("california - %d/%d/%02d %d:%02d ?\n", sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min);

    exit(0);
    */

    time_t now = time(NULL); //взять время в секундах
    struct tm *sp; //разложение времени в нормальный вид

    sp = gmtime(&now); //не локал время а биос условно

    printf("bios\t-\t%d/%d/%02d %d:%02d\n",sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min);

    sp = localtime(&now);

    printf("local\t-\t%d/%d/%02d %d:%02d %s\n",sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min, tzname[sp->tm_isdst]);

    //Можно конечно сменить timezone, но это вариант для умных
    /*
    setenv("TZ", "America/Los_Angeles", 1);
    tzset();

    sp = localtime(&now);

    printf("california\t-%d/%d/%02d %d:%02d %s\n",sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min, tzname[sp->tm_isdst]);
    */

    time_t california = now - 15 * 3600;

    sp = localtime(&california);

    /* Образец
    if(sp->tm_mon >= 2)
    {
        if(sp->tm_wday && sp->tm_mday >= 8)
        {
            if(sp->tm_hour >= 1)
            {
                if(sp->tm_mon <= 10)
                {
                    if(sp->tm_wday == 6 && sp->tm_mday <= 7)
                    {
                        if(sp->tm_hour < 2)
                        {
                            california += 3600;

                            sp = localtime(&california);
                        }
                    }
                }
            }
        }
    }
    */

    if(sp->tm_mon > 2 && sp->tm_mon < 10) //Точно действует
    {
        california += 3600;
        sp = localtime(&california);
    } else if(sp->tm_mon == 2) //левая границы
    {
        //Берём день месяца, скипаем недели, снимаем его номер дня недели, скипаем отставшиеся недели, получаем
        int first_wday = (sp->tm_wday - (sp->tm_mday - 1) % 7 + 7) % 7; //тип первога дня в недели

        int first_sunday = 1 + (7 - first_wday) % 7; //От него считаем первое соксресенье

        if(sp->tm_mday > first_sunday + 7 || (sp->tm_mday == first_sunday + 7 && sp->tm_hour >= 2)) //Если 2е воскоересья в 2 часа ночи
        {
            california += 3600;
            sp = localtime(&california);
        }
    } else if(sp->tm_mon == 10)
    {
        int first_wday = (sp->tm_wday - (sp->tm_mday - 1) % 7 + 7) % 7;

        int first_sunday = 1 + (7 - first_wday) % 7;

        if(sp->tm_mday < first_sunday || (sp->tm_mday == first_sunday && sp->tm_hour < 1)) //
        {
            california += 3600;
            sp = localtime(&california);
        }
    }

    printf("california\t-\t%d/%d/%02d %d:%02d %s\n",sp->tm_mday, sp->tm_mon + 1, sp->tm_year + 1900, sp->tm_hour, sp->tm_min, tzname[sp->tm_isdst]);

    return 0;
}
