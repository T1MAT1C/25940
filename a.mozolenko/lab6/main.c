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
#include <sys/select.h>
#include <unistd.h>

enum input_result {
    INPUT_ERROR = -1,
    INPUT_EOF,
    INPUT_NUMBER,
    INPUT_INVALID,
    INPUT_TIMEOUT
};

struct line {
    off_t offset;
    size_t length;
};

struct line_table {
    struct line *items;
    size_t count;
    size_t capacity;
};

static volatile sig_atomic_t timed_out;

static void
handle_alarm(int signal_number)
{
    (void)signal_number;
    timed_out = 1;
}

static int
append_line(struct line_table *table, off_t offset, size_t length)
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
build_table(int fd, struct line_table *table)
{
    char buffer[4096];
    off_t start = 0;
    size_t length = 0;

    for (;;) {
        ssize_t bytes;
        off_t end;
        off_t block_start;
        size_t i;

        bytes = read(fd, buffer, sizeof(buffer));
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return -1;
        }
        if (bytes == 0) {
            break;
        }

        end = lseek(fd, 0L, SEEK_CUR);
        if (end == (off_t)-1) {
            perror("lseek");
            return -1;
        }
        block_start = end - (off_t)bytes;

        for (i = 0; i < (size_t)bytes; ++i) {
            if (length == SIZE_MAX - 1) {
                fprintf(stderr, "Line is too long\n");
                return -1;
            }
            ++length;
            if (buffer[i] == '\n') {
                if (append_line(table, start, length) == -1) {
                    return -1;
                }
                start = block_start + (off_t)i + 1;
                length = 0;
            }
        }
    }

    if (length != 0) {
        return append_line(table, start, length);
    }
    return 0;
}

static int
print_line(int fd, const struct line *line)
{
    char *text;
    size_t total = 0;
    int result = 0;

    if (lseek(fd, line->offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return -1;
    }
    text = malloc(line->length + 1);
    if (text == NULL) {
        perror("malloc");
        return -1;
    }

    while (total < line->length) {
        size_t remaining = line->length - total;
        size_t chunk = remaining < 4096 ? remaining : 4096;
        ssize_t bytes = read(fd, text + total, chunk);

        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            result = -1;
            break;
        }
        if (bytes == 0) {
            fprintf(stderr, "Unexpected end of file; rebuild the line table\n");
            result = -1;
            break;
        }
        total += (size_t)bytes;
    }

    if (result == 0) {
        text[total] = '\0';
        if (printf("%s", text) < 0) {
            perror("printf");
            result = -1;
        } else if (text[total - 1] != '\n' && printf("\n") < 0) {
            perror("printf");
            result = -1;
        }
    }
    free(text);
    return result;
}

static int
print_file(int fd)
{
    char buffer[4096];

    if (lseek(fd, 0L, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return -1;
    }
    for (;;) {
        ssize_t bytes = read(fd, buffer, sizeof(buffer));

        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return -1;
        }
        if (bytes == 0) {
            return 0;
        }
        if (fwrite(buffer, 1, (size_t)bytes, stdout) != (size_t)bytes) {
            perror("fwrite");
            return -1;
        }
    }
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

        /* Unblock SIGALRM atomically with the wait. */
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

        /* Consume a pending alarm before restoring the original mask. */
        if (sigpending(&pending) == -1) {
            perror("sigpending");
            result = INPUT_ERROR;
        } else if (sigismember(&pending, SIGALRM) == 1) {
            int signal_number;
            int error = sigwait(&alarm_set, &signal_number);

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

int
main(int argc, char *argv[])
{
    struct line_table table = { NULL, 0, 0 };
    struct sigaction action;
    int fd;
    int status = EXIT_SUCCESS;
    size_t i;

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
    if (build_table(fd, &table) == -1) {
        status = EXIT_FAILURE;
    } else {
        printf("Line\tOffset\tLength (bytes, including newline)\n");
        for (i = 0; i < table.count; ++i) {
            printf("%zu\t%jd\t%zu\n", i + 1,
                (intmax_t)table.items[i].offset, table.items[i].length);
        }

        for (;;) {
            size_t number;
            int result;

            if (table.count == 0) {
                printf("File is empty. Enter 0 to quit (5 seconds): ");
            } else {
                printf("Line number (1-%zu, 0 to quit, 5 seconds): ", table.count);
            }
            if (fflush(stdout) == EOF) {
                perror("fflush");
                status = EXIT_FAILURE;
                break;
            }

            result = timed_read_number(table.count, &number);
            if (result == INPUT_ERROR) {
                status = EXIT_FAILURE;
                break;
            }
            if (result == INPUT_TIMEOUT) {
                if (printf("\nTime limit expired. File contents:\n") < 0 ||
                    print_file(fd) == -1) {
                    status = EXIT_FAILURE;
                }
                break;
            }
            if (result == INPUT_EOF ||
                (result == INPUT_NUMBER && number == 0)) {
                break;
            }
            if (result == INPUT_INVALID) {
                fprintf(stderr, "Enter an integer from 0 to %zu\n", table.count);
                continue;
            }
            if (print_line(fd, &table.items[number - 1]) == -1) {
                status = EXIT_FAILURE;
                break;
            }
        }
    }

    free(table.items);
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
