#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <ulimit.h>
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

    if (errno != 0 || str == end || *end != '\0' || result < 0) {
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

    struct option_info *options =
        malloc(sizeof(struct option_info) * (argc > 1 ? argc - 1 : 1));

    if (options == NULL) {
        perror("malloc");
        return 1;
    }

    opterr = 0;

    while ((opt = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {

        if (opt == '?') {
            if (optopt == 'U' || optopt == 'C' || optopt == 'V') {
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

        options[count].option = opt;
        options[count].argument = optarg;
        count++;
    }

    //СПРАВА НАЛЕВО
    for (int i = count - 1; i >= 0; i--) {

        switch (options[i].option) {

        case 'i':
            printf("uid=%ld euid=%ld gid=%ld egid=%ld\n",
                   (long)getuid(),
                   (long)geteuid(),
                   (long)getgid(),
                   (long)getegid());
            break;

        case 's':
            if (setpgid(0, 0) == -1) {
                perror("setpgid");
                status = 1;
            }
            break;

        case 'p':
            printf("pid=%ld ppid=%ld pgid=%ld\n",
                   (long)getpid(),
                   (long)getppid(),
                   (long)getpgrp());
            break;

        case 'u':
        {
            long limit;

            limit = sysconf(_SC_CHILD_MAX);

            if (limit == -1) {
                perror("sysconf");
                status = 1;
            } else {
                printf("%ld\n", limit);
            }

            break;
        }

        case 'U':
        {
            long value;

            if (parse_nonnegative_long(options[i].argument, &value) == -1) {
                fprintf(stderr,
                        "Invalid value for -U: %s\n",
                        options[i].argument);
                status = 1;
                break;
            }

            if (ulimit(UL_SETFSIZE, value) == -1) {
                perror("ulimit");
                status = 1;
            }

            break;
        }

        case 'c':
        {
            struct rlimit limit;

            if (getrlimit(RLIMIT_CORE, &limit) == -1) {
                perror("getrlimit");
                status = 1;
                break;
            }

            if (limit.rlim_max == RLIM_INFINITY) {
                printf("core size: unlimited\n");
            } else {
                printf("core size: %lu bytes\n",
                       (unsigned long)limit.rlim_max);
            }

            break;
        }

        case 'C':
        {
            long value;
            struct rlimit limit;

            if (parse_nonnegative_long(options[i].argument, &value) == -1) {
                fprintf(stderr,
                        "Invalid value for -C: %s\n",
                        options[i].argument);
                status = 1;
                break;
            }

            if (getrlimit(RLIMIT_CORE, &limit) == -1) {
                perror("getrlimit");
                status = 1;
                break;
            }

            limit.rlim_cur = (rlim_t)value;

            if (setrlimit(RLIMIT_CORE, &limit) == -1) {
                perror("setrlimit");
                status = 1;
            }

            break;
        }

        case 'd':
        {
            char cwd[PATH_MAX];

            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                perror("getcwd");
                status = 1;
            } else {
                printf("%s\n", cwd);
            }

            break;
        }

        case 'v':
        {
            char **env;

            for (env = environ; *env != NULL; env++) {
                printf("%s\n", *env);
            }

            break;
        }

        case 'V':
        {
            char *arg = options[i].argument;
            char *equal_sign = strchr(arg, '=');

            if (equal_sign == NULL || equal_sign == arg) {
                fprintf(stderr,
                        "Invalid value for -V: %s\n",
                        arg);
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
