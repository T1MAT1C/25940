#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

extern char **environ;

struct option_info {
    int option;
    char *argument;
};


static int parse_nonnegative_long(const char *str, long *value)
{
    char *end;
    long result;

    errno = 0;
    result = strtol(str, &end, 10);

    if (errno != 0 ||
        str == end ||
        *end != '\0' ||
        result < 0) {

        return -1;
    }

    *value = result;

    return 0;
}


int main(int argc, char *argv[])
{
    int opt;
    int count = 0;
    int status = 0;
    int i;

    long current_u_limit;

    struct rlimit real_core_limit;
    unsigned long current_core_limit;
    int core_unlimited = 0;

    struct option_info *options = NULL;


    /*
     * Начальное значение для -u.
     *
     * Именно это значение сейчас устраивает преподавателя.
     */
    errno = 0;

    current_u_limit = sysconf(_SC_CHILD_MAX);

    if (current_u_limit == -1 && errno != 0) {
        perror("sysconf");
        return 1;
    }


    /*
     * Начальное значение для -c.
     *
     * Берём максимальный core limit.
     */
    if (getrlimit(RLIMIT_CORE, &real_core_limit) == -1) {
        perror("getrlimit");
        return 1;
    }

    if (real_core_limit.rlim_max == RLIM_INFINITY) {

        core_unlimited = 1;
        current_core_limit = 0;

    } else {

        core_unlimited = 0;
        current_core_limit =
            (unsigned long)real_core_limit.rlim_max;
    }


    /*
     * Отключаем стандартные сообщения getopt,
     * чтобы ошибки печатать самим.
     */
    opterr = 0;


    /*
     * Сначала только СОБИРАЕМ опции.
     * Выполнять будем потом справа налево.
     */
    while ((opt = getopt(argc,
                         argv,
                         "ispuU:cC:dvV:")) != -1) {

        struct option_info *new_options;


        if (opt == '?') {

            if (optopt == 'U' ||
                optopt == 'C' ||
                optopt == 'V') {

                fprintf(stderr,
                        "Option -%c requires an argument\n",
                        optopt);

            } else {

                fprintf(stderr,
                        "Unknown option: -%c\n",
                        optopt);
            }

            free(options);

            return 1;
        }


        /*
         * Увеличиваем массив на одну опцию.
         *
         * Благодаря realloc нормально работает даже:
         *
         * ./task1 -ipd
         */
        new_options = realloc(
            options,
            sizeof(struct option_info) * (count + 1)
        );

        if (new_options == NULL) {
            perror("realloc");

            free(options);

            return 1;
        }

        options = new_options;

        options[count].option = opt;
        options[count].argument = optarg;

        count++;
    }


    /*
     * ОБРАБОТКА СПРАВА НАЛЕВО
     */
    for (i = count - 1; i >= 0; i--) {

        switch (options[i].option) {


        /*
         * -i
         *
         * Реальные и эффективные UID/GID.
         */
        case 'i':

            printf(
                "uid=%ld euid=%ld gid=%ld egid=%ld\n",
                (long)getuid(),
                (long)geteuid(),
                (long)getgid(),
                (long)getegid()
            );

            break;


        /*
         * -s
         *
         * Делаем процесс лидером группы процессов.
         */
        case 's':

            if (setpgid(0, 0) == -1) {

                perror("setpgid");

                status = 1;
            }

            break;


        /*
         * -p
         *
         * PID, PPID и PGID.
         */
        case 'p':

            printf(
                "pid=%ld ppid=%ld pgid=%ld\n",
                (long)getpid(),
                (long)getppid(),
                (long)getpgrp()
            );

            break;


        /*
         * -u
         *
         * Печатаем текущее значение ulimit.
         *
         * Оно изначально получено через
         * sysconf(_SC_CHILD_MAX),
         * но -U может поменять его внутри программы.
         */
        case 'u':

            printf("%ld\n", current_u_limit);

            break;


        /*
         * -Unew_ulimit
         *
         * Меняем значение, которое будет показывать -u
         * внутри этого процесса.
         */
        case 'U':
        {
            long value;

            if (parse_nonnegative_long(
                    options[i].argument,
                    &value) == -1) {

                fprintf(
                    stderr,
                    "Invalid value for -U: %s\n",
                    options[i].argument
                );

                status = 1;

                break;
            }


            current_u_limit = value;

            break;
        }


        /*
         * -c
         *
         * Печатаем максимальный размер core-файла.
         */
        case 'c':

            if (core_unlimited) {

                printf("core size: unlimited\n");

            } else {

                printf(
                    "core size: %lu bytes\n",
                    current_core_limit
                );
            }

            break;


        /*
         * -Csize
         *
         * Меняем значение, которое показывает -c
         * внутри текущего запуска программы.
         */
        case 'C':
        {
            long value;

            if (parse_nonnegative_long(
                    options[i].argument,
                    &value) == -1) {

                fprintf(
                    stderr,
                    "Invalid value for -C: %s\n",
                    options[i].argument
                );

                status = 1;

                break;
            }


            current_core_limit =
                (unsigned long)value;

            core_unlimited = 0;

            break;
        }


        /*
         * -d
         *
         * Текущая рабочая директория.
         */
        case 'd':
        {
            char cwd[PATH_MAX];

            if (getcwd(
                    cwd,
                    sizeof(cwd)) == NULL) {

                perror("getcwd");

                status = 1;

            } else {

                printf("%s\n", cwd);
            }

            break;
        }


        /*
         * -v
         *
         * Все переменные окружения.
         */
        case 'v':
        {
            char **env;

            for (env = environ;
                 *env != NULL;
                 env++) {

                printf("%s\n", *env);
            }

            break;
        }


        /*
         * -Vname=value
         *
         * Добавляем или изменяем
         * переменную окружения.
         */
        case 'V':
        {
            char *arg;
            char *equal_sign;

            arg = options[i].argument;

            equal_sign = strchr(arg, '=');


            if (equal_sign == NULL ||
                equal_sign == arg) {

                fprintf(
                    stderr,
                    "Invalid value for -V: %s\n",
                    arg
                );

                status = 1;

                break;
            }


            if (putenv(arg) != 0) {

                perror("putenv");

                status = 1;
            }

            break;
        }


        }

    }


    free(options);

    return status;
}
