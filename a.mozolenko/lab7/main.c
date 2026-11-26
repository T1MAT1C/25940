#define _POSIX_C_SOURCE 200112L
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

struct line {
    size_t offset;
    size_t length;
};

struct table {
    struct line *items;
    size_t count;
    size_t capacity;
};

enum input_result {
    INPUT_ERROR = -1, INPUT_EOF, INPUT_NUMBER, INPUT_INVALID, INPUT_TIMEOUT
};

static volatile sig_atomic_t timed_out;

static void
handle_alarm(int signo)
{
    (void)signo;
    timed_out = 1;
}

static int
append_line(struct table *table, size_t offset, size_t length)
{
    if (table->count == table->capacity) {
        size_t capacity;
        struct line *items;

        if (table->capacity > SIZE_MAX / 2) {
            fprintf(stderr, "Too many lines\n");
            return -1;
        }
        capacity = table->capacity == 0 ? 64 : table->capacity * 2;
        if (capacity > SIZE_MAX / sizeof(*items)) {
            fprintf(stderr, "Line table is too large\n");
            return -1;
        }
        items = realloc(table->items, capacity * sizeof(*items));
        if (items == NULL) {
            perror("realloc");
            return -1;
        }
        table->items = items;
        table->capacity = capacity;
    }
    table->items[table->count].offset = offset;
    table->items[table->count].length = length;
    ++table->count;
    return 0;
}

static int
build_table(const char *data, size_t size, struct table *table)
{
    size_t start = 0;
    size_t i;

    for (i = 0; i < size; ++i) {
        if (data[i] == '\n') {
            if (append_line(table, start, i - start + 1) == -1) {
                return -1;
            }
            start = i + 1;
        }
    }
    return start < size ? append_line(table, start, size - start) : 0;
}

static int
print_bytes(const char *data, size_t size)
{
    if (size != 0 && fwrite(data, 1, size, stdout) != size) {
        perror("fwrite");
        return -1;
    }
    return 0;
}

static int
read_number(size_t count, size_t *number, const sigset_t *wait_mask)
{
    char input[128];
    size_t length = 0;
    int invalid = 0;
    char *p;
    char *end;
    uintmax_t value;

    for (;;) {
        fd_set ready;
        int result;
        char c;
        ssize_t bytes;

        if (timed_out) {
            return INPUT_TIMEOUT;
        }
        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);
        result = pselect(STDIN_FILENO + 1, &ready, NULL, NULL, NULL, wait_mask);
        if (timed_out) {
            return INPUT_TIMEOUT;
        }
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("pselect");
            return INPUT_ERROR;
        }
        if (result == 0) {
            continue;
        }
        bytes = read(STDIN_FILENO, &c, 1);
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read stdin");
            return INPUT_ERROR;
        }
        if (bytes == 0 && length == 0 && !invalid) {
            return INPUT_EOF;
        }
        if (bytes == 0 || c == '\n') {
            break;
        }
        if (c == '\0') {
            invalid = 1;
        }
        if (length < sizeof(input) - 1) {
            input[length++] = c;
        } else {
            invalid = 1;
        }
    }
    input[length] = '\0';
    p = input;
    while (isspace((unsigned char)*p)) {
        ++p;
    }
    if (invalid || *p < '0' || *p > '9') {
        return INPUT_INVALID;
    }
    errno = 0;
    value = strtoumax(p, &end, 10);
    if (errno == ERANGE) {
        return INPUT_INVALID;
    }
    while (isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0' || value > (uintmax_t)count) {
        return INPUT_INVALID;
    }
    *number = (size_t)value;
    return INPUT_NUMBER;
}
static int
timed_read_number(size_t count, size_t *number)
{
    sigset_t alarm_set;
    sigset_t old_mask;
    sigset_t wait_mask;
    sigset_t pending;
    int result;

    if (sigemptyset(&alarm_set) == -1 ||
        sigaddset(&alarm_set, SIGALRM) == -1 ||
        sigprocmask(SIG_BLOCK, &alarm_set, &old_mask) == -1) {
        perror("block SIGALRM");
        return INPUT_ERROR;
    }
    wait_mask = old_mask;
    if (sigdelset(&wait_mask, SIGALRM) == -1) {
        perror("sigdelset");
        result = INPUT_ERROR;
    } else {
        timed_out = 0;
        alarm(5);
        result = read_number(count, number, &wait_mask);
        alarm(0);
        if (sigpending(&pending) == -1) {
            perror("sigpending");
            result = INPUT_ERROR;
        } else if (sigismember(&pending, SIGALRM) == 1) {
            int signo;
            int error = sigwait(&alarm_set, &signo);

            if (error != 0) {
                fprintf(stderr, "sigwait: %s\n", strerror(error));
                result = INPUT_ERROR;
            } else if (result != INPUT_ERROR) {
                result = INPUT_TIMEOUT;
            }
        }
    }
    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        perror("restore signal mask");
        return INPUT_ERROR;
    }
    return result;
}

static int
lookup(const char *data, size_t size, const struct table *table)
{
    size_t i;

    if (printf("Line\tOffset\tLength (bytes, including newline)\n") < 0) {
        return -1;
    }
    for (i = 0; i < table->count; ++i) {
        if (printf("%zu\t%zu\t%zu\n", i + 1, table->items[i].offset,
            table->items[i].length) < 0) {
            return -1;
        }
    }
    for (;;) {
        size_t number;
        int result;
        const struct line *line;

        if (printf("Line number (0-%zu, 0 to quit, 5 seconds): ",
            table->count) < 0 || fflush(stdout) == EOF) {
            perror("stdout");
            return -1;
        }
        result = timed_read_number(table->count, &number);
        if (result == INPUT_ERROR) {
            return -1;
        }
        if (result == INPUT_TIMEOUT) {
            if (printf("\nTime limit expired. File contents:\n") < 0) {
                return -1;
            }
            return print_bytes(data, size);
        }
        if (result == INPUT_EOF || (result == INPUT_NUMBER && number == 0)) {
            return 0;
        }
        if (result == INPUT_INVALID) {
            fprintf(stderr, "Enter an integer from 0 to %zu\n", table->count);
            continue;
        }
        line = &table->items[number - 1];
        if (print_bytes(data + line->offset, line->length) == -1) {
            return -1;
        }
        if (data[line->offset + line->length - 1] != '\n' &&
            putchar('\n') == EOF) {
            perror("putchar");
            return -1;
        }
    }
}

int
main(int argc, char *argv[])
{
    struct table table = { NULL, 0, 0 };
    struct stat info;
    struct sigaction action;
    const char *data = NULL;
    void *mapping = MAP_FAILED;
    size_t size = 0;
    int fd;
    int status = EXIT_FAILURE;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_alarm;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGALRM, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    if (fstat(fd, &info) == -1) {
        perror("fstat");
        goto cleanup;
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0 ||
        (uintmax_t)info.st_size > (uintmax_t)SIZE_MAX) {
        fprintf(stderr, "A regular file fitting in size_t is required\n");
        goto cleanup;
    }
    size = (size_t)info.st_size;
    if (size != 0) {
        mapping = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED) {
            perror("mmap");
            goto cleanup;
        }
        data = mapping;
    }
    if (build_table(data, size, &table) == -1 ||
        lookup(data, size, &table) == -1) {
        goto cleanup;
    }
    status = EXIT_SUCCESS;

cleanup:
    free(table.items);
    if (mapping != MAP_FAILED && munmap(mapping, size) == -1) {
        perror("munmap");
        status = EXIT_FAILURE;
    }
    if (close(fd) == -1) {
        perror("close");
        status = EXIT_FAILURE;
    }
    if (fflush(stdout) == EOF) {
        perror("fflush");
        status = EXIT_FAILURE;
    }
    return status;
}
