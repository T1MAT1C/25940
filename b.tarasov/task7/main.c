#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#ifndef MODE
#define MODE 5
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#if MODE == 7
#include <sys/mman.h>
#endif

#if MODE >= 6
#include <signal.h>
#include <sys/select.h>
#endif

struct line {
    off_t offset;
    off_t length;
};

static struct line *table;
static size_t count, capacity;
static int fd;
static off_t file_size;

#if MODE == 7
static char *mapping;
#endif

static void fail(const char *message)
{
    perror(message);
    exit(1);
}

static void add_line(off_t offset, off_t length)
{
    if (count == capacity) {
        size_t next = capacity ? capacity * 2 : 64;
        struct line *new_table;

        if (next < capacity ||
            next > SIZE_MAX / sizeof(*table)) {
            fprintf(stderr, "Too many lines\n");
            exit(1);
        }

        new_table = realloc(table, next * sizeof(*table));
        if (!new_table)
            fail("realloc");

        table = new_table;
        capacity = next;
    }

    table[count].offset = offset;
    table[count].length = length;
    ++count;
}

static void index_file(void)
{
    struct stat st;
    off_t start = 0;

    if (fstat(fd, &st) < 0)
        fail("fstat");

    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
        fprintf(stderr, "Expected a regular text file\n");
        exit(1);
    }

#if MODE == 7
    size_t i, size;

    if ((uintmax_t)st.st_size > SIZE_MAX) {
        fprintf(stderr, "File is too large to map\n");
        exit(1);
    }

    file_size = st.st_size;
    size = (size_t)file_size;

    if (!size)
        return;

    mapping = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED)
        fail("mmap");

    for (i = 0; i < size; ++i) {
        if (mapping[i] == '\n') {
            off_t end = (off_t)i + 1;
            add_line(start, end - start);
            start = end;
        }
    }
#else
    char buffer[4096];

    for (;;) {
        ssize_t n, i;
        off_t end, base;

        n = read(fd, buffer, sizeof(buffer));

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fail("read file");
        }

        if (!n)
            break;

        end = lseek(fd, 0, SEEK_CUR);
        if (end == (off_t)-1)
            fail("lseek");

        base = end - n;

        for (i = 0; i < n; ++i) {
            if (buffer[i] == '\n') {
                off_t next = base + i + 1;
                add_line(start, next - start);
                start = next;
            }
        }

        file_size = end;
    }
#endif

    if (start < file_size)
        add_line(start, file_size - start);
}

static void print_range(off_t offset, off_t length,
                        int finish_line)
{
    int last = '\n';

#if MODE != 7
    char buffer[4096];

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1)
        fail("lseek");
#endif

    while (length > 0) {
        size_t amount = length > 4096 ? 4096 : (size_t)length;

#if MODE == 7
        const char *buffer = mapping + (size_t)offset;
        size_t n = amount;
#else
        ssize_t n = read(fd, buffer, amount);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fail("read file");
        }

        if (!n) {
            fprintf(stderr, "File changed after indexing\n");
            exit(1);
        }
#endif

        /* The input is a text file, without embedded NUL bytes. */
        if (printf("%.*s", (int)n, buffer) < 0)
            fail("stdout");

        last = (unsigned char)buffer[n - 1];
        offset += (off_t)n;
        length -= (off_t)n;
    }

    if (finish_line && last != '\n')
        putchar('\n');
}

#if MODE >= 6
static volatile sig_atomic_t expired;
static sigset_t alarm_set, wait_mask;

static void on_alarm(int signo)
{
    (void)signo;
    expired = 1;
}

static void setup_alarm(void)
{
    struct sigaction action;

    sigemptyset(&alarm_set);
    sigaddset(&alarm_set, SIGALRM);

    if (sigprocmask(SIG_BLOCK, &alarm_set, &wait_mask) < 0)
        fail("sigprocmask");

    sigdelset(&wait_mask, SIGALRM);

    memset(&action, 0, sizeof(action));
    action.sa_handler = on_alarm;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGALRM, &action, NULL) < 0)
        fail("sigaction");
}

static void cancel_alarm(void)
{
    sigset_t pending;
    int signo;

    alarm(0);

    if (sigpending(&pending) < 0)
        fail("sigpending");

    /* Consume a timer signal that became pending just before cancellation. */
    if (sigismember(&pending, SIGALRM)) {
        int error = sigwait(&alarm_set, &signo);

        if (error) {
            errno = error;
            fail("sigwait");
        }
    }
}
#endif

/* 0 = EOF; 1 = input; 2 = timeout; 3 = input too long. */
static int read_request(char *text, size_t size)
{
    size_t used = 0;
    int too_long = 0;
    int result;

#if MODE >= 6
    static int first_request = 1;

    expired = 0;

    if (first_request) {
        alarm(5);
        first_request = 0;
    }
#endif
    for (;;) {
        char c;
        ssize_t n;

#if MODE >= 6
        fd_set ready;
        int rc;

        if (expired) {
            result = 2;
            break;
        }

        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);

        /* Unblock SIGALRM atomically while waiting for input. */
        rc = pselect(STDIN_FILENO + 1, &ready,
                     NULL, NULL, NULL, &wait_mask);

        if (rc < 0) {
            if (errno == EINTR)
                continue;
            fail("pselect");
        }

        if (expired) {
            result = 2;
            break;
        }
#endif

        n = read(STDIN_FILENO, &c, 1);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fail("read stdin");
        }

        if (!n || c == '\n') {
            text[used] = '\0';
            result = too_long ? 3 : (!n && !used ? 0 : 1);
            break;
        }

        if (used + 1 < size)
            text[used++] = c;
        else
            too_long = 1;
    }

#if MODE >= 6
    cancel_alarm();
#endif

    return result;
}

static int parse_number(const char *text, uintmax_t *number)
{
    char *end;

    while (isspace((unsigned char)*text))
        ++text;

    if (!isdigit((unsigned char)*text))
        return -1;

    errno = 0;
    *number = strtoumax(text, &end, 10);

    if (errno)
        return -1;

    while (isspace((unsigned char)*end))
        ++end;

    return *end ? -1 : 0;
}

int main(int argc, char **argv)
{
    size_t i;

    if (argc < 2 || argc > 3 ||
        (argc == 3 && strcmp(argv[2], "--table"))) {
        fprintf(stderr, "Usage: %s file [--table]\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        fail(argv[1]);

    index_file();

    if (argc == 3) {
        for (i = 0; i < count; ++i) {
            printf("line=%zu offset=%jd length=%jd\n",
                   i + 1,
                   (intmax_t)table[i].offset,
                   (intmax_t)table[i].length);
        }
        goto cleanup;
    }

#if MODE >= 6
    setup_alarm();
#endif

    for (;;) {
        char input[128];
        uintmax_t number;
        int result;

        printf("Line number (1..%zu, 0 to exit): ", count);

        if (fflush(stdout) == EOF)
            fail("stdout");

        result = read_request(input, sizeof(input));

        if (!result)
            break;

        if (result == 2) {
            puts("\nTimeout: printing the whole file.");
            print_range(0, file_size, 0);
            break;
        }

        if (result == 3 || parse_number(input, &number) < 0) {
            puts("Enter a nonnegative integer.");
            continue;
        }

        if (!number)
            break;

        if (number > count) {
            puts("No such line.");
            continue;
        }

        i = (size_t)number - 1;
        print_range(table[i].offset, table[i].length, 1);
    }

cleanup:
    if (fflush(stdout) == EOF)
        fail("stdout");

#if MODE == 7
    if (file_size && munmap(mapping, (size_t)file_size) < 0)
        fail("munmap");
#endif

    free(table);

    if (close(fd) < 0)
        fail("close");

    return 0;
}
