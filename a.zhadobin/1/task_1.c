#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <ulimit.h>
#include <errno.h>
#include <rctl.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

extern char **environ;

struct Operation {
    char option;
    char *arg;
};

void print_safe(const char *str)
{
    const unsigned char *p =
        (const unsigned char *)str;

    while (*p) {
        if (*p >= 32 && *p <= 126) {
            putchar(*p);
        }
        p++;
    }

    putchar('\n');
}

void print_help(const char *program)
{
    printf("\n");
    printf("========================================\n");
    printf("          PROGRAM INFORMATION\n");
    printf("========================================\n");
    printf("Usage: %s [options]\n\n", program);

    printf("Available options:\n");
    printf("  -i         Show UID, EUID, GID, EGID\n");
    printf("  -s         Set process group\n");
    printf("  -p         Show PID, PPID, PGRP\n");
    printf("  -u         Show process limit\n");
    printf("  -U NUMBER  Set process limit\n");
    printf("  -c         Show core size limit\n");
    printf("  -C NUMBER  Set core size limit\n");
    printf("  -d         Show current directory\n");
    printf("  -v         Show environment variables\n");
    printf("  -V NAME=VALUE  Set environment variable\n");

    printf("\nExamples:\n");
    printf("  %s -i -p\n", program);
    printf("  %s -u -U 100\n", program);
    printf("  %s -C 1024\n", program);
    printf("  %s -V TEST=123\n", program);

    printf("========================================\n\n");
}

/* Check numeric arguments */
int check_number(const char *str)
{
    unsigned long long value;
    char *end;

    if (str == NULL || *str == '\0')
        return 0;

    for (const unsigned char *p =
         (const unsigned char *)str; *p; p++) {
        if (*p < '0' || *p > '9')
            return 0;
    }

    errno = 0;
    value = strtoull(str, &end, 10);

    if (errno == ERANGE || *end != '\0' ||
        value > LONG_MAX)
        return 0;

    return 1;
}

/* Check environment variable NAME=VALUE */
int check_env(const char *str)
{
    const unsigned char *p =
        (const unsigned char *)str;

    if (p == NULL || !(*p == '_' ||
        (*p >= 'A' && *p <= 'Z') ||
        (*p >= 'a' && *p <= 'z')))
        return 0;

    while (*p && *p != '=') {
        if (!(*p == '_' ||
              (*p >= 'A' && *p <= 'Z') ||
              (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9')))
            return 0;
        p++;
    }

    if (*p != '=')
        return 0;

    p++;

    while (*p) {
        if (!(isalnum(*p) ||
              *p == '_' || *p == '-' ||
              *p == '.' || *p == '/' ||
              *p == ':' || *p == '@'))
            return 0;
        p++;
    }

    return 1;
}

/* Check all command-line arguments */
int check_arguments(int argc, char *argv[])
{
    int opt;

    opterr = 0;

    while ((opt = getopt(argc, argv,
            ":ispuU:cC:dvV:")) != -1) {

        if (opt == '?' || opt == ':') {
            fprintf(stderr,
                "Error: invalid or missing option\n");
            return -1;
        }

        if (opt == 'U' || opt == 'C') {
            if (!check_number(optarg)) {
                fprintf(stderr,
                    "Error: -%c requires a valid "
                    "non-negative integer\n", opt);
                return -1;
            }
        }

        if (opt == 'V') {
            if (!check_env(optarg)) {
                fprintf(stderr,
                    "Error: invalid environment "
                    "variable format\n");
                return -1;
            }
        }
    }

    if (optind < argc) {
        fprintf(stderr,
            "Error: unexpected argument\n");
        return -1;
    }

    /* Reset getopt for the original main loop */
    optind = 1;
    opterr = 1;

    return 0;
}



int print_ulimit_u_solaris(void)
{
    long max_procs;

    errno = 0;
    max_procs = sysconf(_SC_CHILD_MAX);

    if (max_procs == -1) {
        if (errno != 0) {
            perror("sysconf(_SC_CHILD_MAX)");
            return -1;
        }
        printf("ulimit -u: unlimited\n");
    } else {
        printf("ulimit -u: %ld\n", max_procs);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    print_help(argv[0]);

    if (check_arguments(argc, argv) != 0) {
        fprintf(stderr,
                "Use only permitted options and characters.\n");
        return 1;
    }

    int opt;
    long max_procs = sysconf(_SC_CHILD_MAX), need = 29995;
    struct Operation operations[argc];
    int count = 0;
    while ((opt = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {

        if (opt == '?') {
            continue;
        }

        if (opt == 'U' || opt == 'C' || opt == 'V') {
            operations[count].option = opt;
            operations[count].arg = optarg;
            count += 1;
        } else {
            operations[count].option = opt;
            operations[count].arg = NULL;
            count += 1;
        }
    }
    
    for (int i = count - 1; i >= 0; i--) {
        // max_procs = sysconf(_SC_CHILD_MAX);
        // if (operations[i].option){
        //     max_procs = need;
        // }
        
        if (operations[i].option == 'i') {

            printf("uid   %d\n", getuid());
            printf("euid  %d\n", geteuid());
            printf("gid   %d\n", getgid());
            printf("egid  %d\n", getegid());

        } else if (operations[i].option == 's') {
            setpgid(0, 0);

        } else if (operations[i].option == 'p') {

            printf("pid   %d\n", getpid());
            printf("ppid  %d\n", getppid());
            printf("pgrp  %d\n", getpgrp());

        } else if (operations[i].option == 'u') {
            max_procs = sysconf(_SC_CHILD_MAX);
            errno = 0;
            if (max_procs == -1) {
                if (errno != 0) {
                    perror("sysconf(_SC_CHILD_MAX)");
                    return -1;
                }
                printf("ulimit -u: unlimited\n");
            } else {
                printf("ulimit -u: %ld\n", max_procs);
            }

        } else if (operations[i].option == 'U') {
            // long new_limit = atol(optarg);
            // ulimit(UL_SETFSIZE, new_limit);
            // struct rlimit limit;
            // long value = strtol(operations[i].arg, NULL, 10);
            // getrlimit(RLIMIT_NPROC, &limit);

            // limit.rlim_cur = value;
            // setrlimit(RLIMIT_NPROC, &limit);
            char *end;
            errno = 0;
            long new_limit = strtol(operations[i].arg, &end, 10);

            if (errno != 0 || end == operations[i].arg || *end != '\0' || new_limit <= 0) {
                fprintf(stderr, "invalid -U value: '%s'\n", operations[i].arg);
            } else {
                char cmd[256];
                snprintf(cmd, sizeof(cmd), "prctl -n task.max-processes -v %ld %d >/dev/null 2>&1", new_limit, (int)getpid());
                if (system(cmd) != 0) fprintf(stderr, "set_ulimit: prctl failed\n");
            }
        } else if (operations[i].option == 'c') {

            struct rlimit limit;
            getrlimit(RLIMIT_CORE, &limit);
            printf("core size  %llu bytes\n", (unsigned long long)limit.rlim_cur);

        } else if (operations[i].option == 'C') {

            struct rlimit limit;
            long value = strtol(operations[i].arg, NULL, 10);
            getrlimit(RLIMIT_CORE, &limit);

            limit.rlim_cur = value;
            setrlimit(RLIMIT_CORE, &limit);

        } else if (operations[i].option == 'd') {

            char cwd[1000];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);

        } else if (operations[i].option == 'v') {

            for (char **env = environ; *env != NULL; env++){
                print_safe(*env);
            }
    
        } else if (operations[i].option == 'V') {
            putenv(operations[i].arg);
        }

    }

    return 0;
}
