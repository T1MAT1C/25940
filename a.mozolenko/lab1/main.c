#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s [-i] [-s] [-p] [-u] [-U size] [-c] [-C size] [-d] [-v] [-V name=value]\n", program);
}

static int print_rlimit(int resource, const char *name) {
    struct rlimit limit;
    if (getrlimit(resource, &limit) == -1) { perror("getrlimit"); return -1; }
    printf("%s: ", name);
    if (limit.rlim_cur == RLIM_INFINITY) { puts("unlimited"); }
    else { printf("%ju bytes\n", (uintmax_t)limit.rlim_cur); }
    return 0;
}

static int parse_rlim(const char *text, rlim_t *result) {
    const unsigned char *p;
    char *end;
    uintmax_t value;
    rlim_t converted;
    if (text[0] == '\0') return -1;
    for (p = (const unsigned char *)text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') return -1;
    }
    errno = 0;
    value = strtoumax(text, &end, 10);
    if (errno == ERANGE || *end != '\0') return -1;
    converted = (rlim_t)value;
    if ((uintmax_t)converted != value || converted == RLIM_INFINITY) return -1;
    *result = converted;
    return 0;
}

static int set_rlimit_value(int resource, const char *text, const char *name) {
    struct rlimit limit;
    rlim_t value;
    if (parse_rlim(text, &value) == -1) {
        fprintf(stderr, "Invalid value for %s: '%s'\n", name, text);
        return -1;
    }
    if (getrlimit(resource, &limit) == -1) { perror("getrlimit"); return -1; }
    limit.rlim_cur = value;
    if (setrlimit(resource, &limit) == -1) { perror("setrlimit"); return -1; }
    printf("%s set to %ju bytes\n", name, (uintmax_t)value);
    return 0;
}

static int set_environment(const char *assignment) {
    const char *equals;
    size_t name_length;
    char *name;
    int result;
    equals = strchr(assignment, '=');
    if (equals == NULL || equals == assignment) {
        fprintf(stderr, "Invalid environment assignment: '%s' (expected name=value)\n", assignment);
        return -1;
    }
    name_length = (size_t)(equals - assignment);
    name = malloc(name_length + 1);
    if (name == NULL) { perror("malloc"); return -1; }
    memcpy(name, assignment, name_length);
    name[name_length] = '\0';
    result = setenv(name, equals + 1, 1);
    if (result == -1) { perror("setenv"); free(name); return -1; }
    printf("Environment variable '%s' set to '%s'\n", name, equals + 1);
    free(name);
    return 0;
}

int main(int argc, char *argv[]) {
    int option;
    int status = 0;
    const char *optstring = ":ispU:ucC:dvV:";
    opterr = 0;
    while ((option = getopt(argc, argv, optstring)) != -1) {
        switch (option) {
        case 'i':
            printf("Real UID: %lu, Effective UID: %lu\n", (unsigned long)getuid(), (unsigned long)geteuid());
            printf("Real GID: %lu, Effective GID: %lu\n", (unsigned long)getgid(), (unsigned long)getegid());
            break;
        case 's':
            if (setpgid(0, 0) == -1) { perror("setpgid"); status = 1; }
            else { printf("Process is a process-group leader (PGID: %ld)\n", (long)getpgrp()); }
            break;
        case 'p':
            printf("PID: %ld, PPID: %ld, PGID: %ld\n", (long)getpid(), (long)getppid(), (long)getpgrp());
            break;
        case 'u':
            if (print_rlimit(RLIMIT_FSIZE, "File size limit (ulimit)") == -1) status = 1;
            break;
        case 'U':
            if (set_rlimit_value(RLIMIT_FSIZE, optarg, "File size limit") == -1) status = 1;
            break;
        case 'c':
            if (print_rlimit(RLIMIT_CORE, "Core file size limit") == -1) status = 1;
            break;
        case 'C':
            if (set_rlimit_value(RLIMIT_CORE, optarg, "Core file size limit") == -1) status = 1;
            break;
        case 'd': {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) { perror("getcwd"); status = 1; }
            else { printf("Current working directory: %s\n", cwd); }
            break;
        }
        case 'v': {
            char **entry;
            puts("Environment variables");
            if (environ != NULL) { for (entry = environ; *entry != NULL; ++entry) puts(*entry); }
            puts("-");
            break;
        }
        case 'V':
            if (set_environment(optarg) == -1) status = 1;
            break;
        case ':':
            fprintf(stderr, "Option -%c requires an argument\n", optopt); usage(argv[0]); return 1;
        case '?':
        default:
            if (optopt != '\0') fprintf(stderr, "Invalid option: -%c\n", optopt);
            else fprintf(stderr, "Invalid option\n");
            usage(argv[0]); return 1;
        }
    }
    if (optind < argc) { fprintf(stderr, "Unexpected operand: '%s'\n", argv[optind]); usage(argv[0]); return 1; }
    if (argc == 1) puts("No arguments supplied. Use options to display process attributes.");
    return status;
}