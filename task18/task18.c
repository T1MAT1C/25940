
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>

// имя файла без пути
void print_name(char *path) {
    int len = strlen(path);

    while (len > 1 && path[len - 1] == '/')
        len--;

    if (len == 1 && path[0] == '/') {
        printf("/");
        return;
    }

    int start = len;
    while (start > 0 && path[start - 1] != '/')
        start--;

    printf("%.*s", len - start, path + start);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Использование: %s файл...\n", argv[0]);
        return 1;
    }

    printf("%-10s %5s %-15s %-15s %10s %-17s %s\n",
           "Права", "Связи", "Владелец", "Группа",
           "Размер", "Дата", "Имя");

    for (int i = 1; i < argc; i++) {
        struct stat st;

        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            continue;
        }

        // Определяем тип файла и права доступа
        char mode[11] = "----------";

        if (S_ISDIR(st.st_mode))    //каталог
            mode[0] = 'd';
        else if (S_ISREG(st.st_mode))   // обычный файл
            mode[0] = '-';
        else
            mode[0] = '?';  //  гав

        if (st.st_mode & S_IRUSR) mode[1] = 'r';
        if (st.st_mode & S_IWUSR) mode[2] = 'w';
        if (st.st_mode & S_IXUSR) mode[3] = 'x';

        if (st.st_mode & S_IRGRP) mode[4] = 'r';
        if (st.st_mode & S_IWGRP) mode[5] = 'w';
        if (st.st_mode & S_IXGRP) mode[6] = 'x';

        if (st.st_mode & S_IROTH) mode[7] = 'r';
        if (st.st_mode & S_IWOTH) mode[8] = 'w';
        if (st.st_mode & S_IXOTH) mode[9] = 'x';

        // владелец и группа
        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);

        char owner[128], group[128];

        snprintf(owner, sizeof(owner), "%s",
                 pw ? pw->pw_name : "unknown");
        snprintf(group, sizeof(group), "%s",
                 gr ? gr->gr_name : "unknown");

        // получаем дату изменения файла
        char date[32] = "unknown";
        struct tm *tm = localtime(&st.st_mtime);

        if (tm)
            strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

        // принт принт принт
        printf("%-10s %5lu %-15s %-15s",
               mode, (unsigned long)st.st_nlink, owner, group);

        if (S_ISREG(st.st_mode))
            printf(" %10lld", (long long)st.st_size);
        else
            printf(" %10s", "");

        printf(" %-17s ", date);
        print_name(argv[i]);
        printf("\n");
    }

    return 0;
}