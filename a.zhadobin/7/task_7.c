
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

struct Line {
    int start;
    int len;
};

struct Line table[200];
int file_r, n, line_start = 0, line_count = 0;

char *file_memory;
int file_size;

volatile sig_atomic_t alarm_triggered = 0;

void print_help(void)
{
    printf("\n====================================\n");
    printf("       FILE READER WITH MMAP\n");
    printf("====================================\n");
    printf("This program reads a text file\n");
    printf("using memory mapping (mmap).\n");
    printf("It creates a table of line positions\n");
    printf("and allows you to display any line.\n");

    printf("\nCommands:\n");
    printf("  help   - show this information\n");
    printf("  NUMBER - display selected line\n");
    printf("  0      - exit program\n");

    printf("\nRules:\n");
    printf("  Enter only positive integers\n");
    printf("  Special characters are forbidden\n");
    printf("  Invalid input will be skipped\n");
    printf("  You have 5 seconds to enter a number\n");
    printf("  After timeout all lines are printed\n");
    printf("====================================\n\n");
}

int check_input(const char *str)
{
    if (*str == '\0') {
        return 0;
    }

    while (*str != '\0') {
        if (*str < '0' || *str > '9') {
            return 0;
        }
        str++;
    }

    return 1;
}

void finish(int sig)
{
    (void)sig;
    alarm_triggered = 1;
}

void print_all_lines(void)
{
    printf("\nclosed\n");

    for (int i = 0; i < line_count; i++) {
        printf("%.*s",
            table[i].len,
            file_memory + table[i].start);
    }
}

int main(int argc, char *argv[])
{
    char filename[256];

    print_help();

    if (argc == 2) {
        if (strlen(argv[1]) >= sizeof(filename)) {
            printf("Error: filename is too long!\n");
            return 1;
        }

        strcpy(filename, argv[1]);

    } else if (argc == 1) {
        printf("Enter filename: ");
        fflush(stdout);

        if (fgets(filename, sizeof(filename), stdin) == NULL) {
            return 1;
        }

        filename[strcspn(filename, "\n")] = '\0';

        if (filename[0] == '\0') {
            printf("Error: empty filename!\n");
            return 1;
        }
    } else {
        printf("Error: too many arguments!\n");
        return 1;
    }

    file_r = open(filename, O_RDONLY);

    if (file_r == -1) {
        perror("open");
        return 1;
    }

    struct stat st;

    if (fstat(file_r, &st) == -1) {
        perror("fstat");
        close(file_r);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        printf("Error: not a regular file!\n");
        close(file_r);
        return 1;
    }

    if (st.st_size == 0) {
        printf("Error: file is empty!\n");
        close(file_r);
        return 1;
    }

    if (st.st_size > INT_MAX) {
        printf("Error: file is too large!\n");
        close(file_r);
        return 1;
    }

    file_size = st.st_size;

    file_memory = mmap(NULL, file_size,
                       PROT_READ, MAP_PRIVATE,
                       file_r, 0);

    if (file_memory == MAP_FAILED) {
        perror("mmap");
        close(file_r);
        return 1;
    }

    table[0].start = 0;

    char c;

    while ((n = read(file_r, &c, 1)) > 0) {

        if (c == '\n') {
            int current_position =
                lseek(file_r, 0, SEEK_CUR);

            if (current_position == -1) {
                perror("lseek");
                munmap(file_memory, file_size);
                close(file_r);
                return 1;
            }

            table[line_count].len =
                current_position - line_start;

            line_count++;
            line_start = current_position;

            if (line_count >= 200) {
                printf("Error: too many lines!\n");
                munmap(file_memory, file_size);
                close(file_r);
                return 1;
            }

            table[line_count].start = line_start;
        }
    }

    if (n == -1) {
        perror("read");
        munmap(file_memory, file_size);
        close(file_r);
        return 1;
    }

    int end = lseek(file_r, 0L, SEEK_CUR);

    if (end == -1) {
        perror("lseek");
        munmap(file_memory, file_size);
        close(file_r);
        return 1;
    }

    table[line_count].len = end - line_start;
    line_count++;

    printf("\nresult:\n");
    printf("Line\nstart\tlen\n");

    for (int i = 0; i < line_count; i++) {
        printf("%d\t%d\t%d\n",
            i + 1, table[i].start, table[i].len);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = finish;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        munmap(file_memory, file_size);
        close(file_r);
        return 1;
    }
    alarm(5);

    while (1) {
        char input[100];
        int line_number;

        alarm_triggered = 0;


        printf("\ninput: ");
        fflush(stdout);

        errno = 0;

        if (fgets(input, sizeof(input), stdin) == NULL) {
            alarm(0);

            if (alarm_triggered) {
                clearerr(stdin);
                print_all_lines();
                break;
            }

            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }

            break;
        }

        alarm(0);

        if (alarm_triggered) {
            print_all_lines();
            break;
        }

        int input_len = strlen(input);

        if (input_len > 0 &&
            input[input_len - 1] == '\n') {
            input[input_len - 1] = '\0';
        } else if (input_len == sizeof(input) - 1) {
            int ch;

            while ((ch = getchar()) != '\n'
                   && ch != EOF) {
            }

            printf("Error: input is too long!\n");
            continue;
        }

        if (strcmp(input, "help") == 0) {
            print_help();
            continue;
        }

        if (!check_input(input)) {
            printf("Error: invalid input!\n");
            printf("Enter a valid line number.\n");
            continue;
        }

        errno = 0;
        char *endptr;
        long number = strtol(input, &endptr, 10);

        if (errno == ERANGE || number > INT_MAX) {
            printf("Error: number is too large!\n");
            continue;
        }

        line_number = (int)number;

        if (line_number == 0) {
            break;
        }

        if (line_number < 1 ||
            line_number > line_count) {
            printf("Error: line does not exist!\n");
            printf("Valid range: 1 - %d\n",
                   line_count);
            continue;
        }

        int len = table[line_number - 1].len;

        printf("%.*s", len,
            file_memory + table[line_number - 1].start);
    }

    alarm(0);
    munmap(file_memory, file_size);
    close(file_r);

    return 0;
}
