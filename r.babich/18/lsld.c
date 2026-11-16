#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/resource.h>
#include <ulimit.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern char *optarg; 
extern int optind, opterr, optopt;

void print_size(long size, int flag)
{
    if(!flag || size < 1024)
    {
        printf("%ld\t", size);
        return;
    }

    const char *units[] = {"B", "K", "M", "G", "T", "P", "E"};

    double value = (double)size;
    int i = 0;

    while(value >= 1024 && i < 6)
    {
        value /= 1024;
        i++;
    }

    printf("%.1f%s\t", value, units[i]);
}

int main(int argc, char *argv[])
{
    const char *optstring = "ahintrL";

    unsigned char options = 0b00000000;

    char *path = NULL;

    int flag;

    while((flag = getopt(argc, argv, optstring)) != -1)
    {
        switch(flag)
        {
            case 'a':
                {
                    options |= 0b10000000;
                }

                break;
          
            case 'h':
                {
                    options |= 0b01000000;
                }

                break;            

            case 'i':
                {
                    options |= 0b00100000;
                }

                break;

            case 'n':
                {
                    options |= 0b00010000;
                }

                break;
                
            case 't':
                {
                    options |= 0b00001000;
                }

                break;    
                
            case 'r':
                {
                    options |= 0b00000100;
                }

                break;                

            case 'L':
                {
                    options |= 0b0000001;
                }

                break;
            
            case '?':
                {

                }
        }
    }

    if(optind < argc)
    {
        path = argv[optind];
    } else {
        path = ".";
    }

    struct stat st;

    if(options & 0b00000001) //Флаг -L
    {
        if(stat(path, &st) == -1)
        {
            perror("stat");
            return 1;
        }
    } else {
        if(lstat(path, &st) == -1)
        {
            perror("lstat");
            return 1;
        }
    }

    if(options & 0b00100000) //Флаг -i
    {
        printf("%lu\t", st.st_ino);
    }

    char permissions[11] = "----------";

    if(S_ISDIR(st.st_mode))
    {
        permissions[0] = 'd';
    } else if(S_ISLNK(st.st_mode))
    {
        permissions[0] = 'l';
    } else if(S_ISCHR(st.st_mode))
    {
        permissions[0] = 'c';
    } else if(S_ISBLK(st.st_mode))
    {
        permissions[0] = 'b';
    } else if(S_ISFIFO(st.st_mode))
    {
        permissions[0] = 'p';
    } else if(S_ISSOCK(st.st_mode))
    {
        permissions[0] = 's';
    }

    permissions[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
    permissions[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
    permissions[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';

    permissions[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
    permissions[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
    permissions[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';

    permissions[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
    permissions[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
    permissions[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';

    printf("%s\t", permissions); //Права

    printf("%lu\t", st.st_nlink); //Кол-ва файлов (условно)

    if(options & 0b00010000) //Флаг -n
    {
        printf("%u %u ", st.st_uid, st.st_gid);
    } else {
        struct passwd *pw = getpwuid(st.st_uid); //владелец
        struct group *gr = getgrgid(st.st_gid); //группа

        char *own = NULL;
        char *grn = NULL;

        if(pw != NULL)
        {
            own = pw->pw_name;
        }

        if(gr != NULL)
        {
            grn = gr->gr_name;
        }

        printf("%s\t%s\t", own, grn); //Владелец и группа
    }

    print_size(st.st_size, options & 0b01000000); //Размер + флаг -h

    struct tm *tm = localtime(&st.st_mtime);

    if(tm == NULL)
    {
        perror("localtime");
        return 1;
    }

    char timebuf[64];

    strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm);

    printf("%s\t", timebuf); //Время последнего изменения

    printf("%s\n", path);

    return 0;
}