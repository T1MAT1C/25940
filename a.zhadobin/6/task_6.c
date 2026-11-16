
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

struct Line {
    int start;
    int len;
};

struct Line table[200];
int file_r, n, line_start = 0, line_count = 0;

void print_help(void)
{
    printf("\n====================================\n");
    printf("        FILE LINE READER + ALARM\n");
    printf("====================================\n");
    printf("This program reads a text file.\n");
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
    char *buffer;

    /* Keep SIGALRM from interrupting output */
    alarm(0);

    printf("\nclosed\n");

    for (int i = 0; i < line_count; i++) {
        buffer = malloc(table[i].len + 1);

        lseek(file_r, table[i].start, SEEK_SET);

        int n = read(file_r, buffer, table[i].len);
        buffer[n] = '\0';

        printf("%s", buffer);

        free(buffer);
    }

    close(file_r);
    exit(0);
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
        perror("Error opening file");
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
                close(file_r);
                return 1;
            }

            table[line_count].len =
                current_position - line_start;

            line_count += 1;
            line_start = current_position;

            if (line_count >= 200) {
                printf("Error: too many lines!\n");
                close(file_r);
                return 1;
            }

            table[line_count].start = line_start;
        }
    }

    if (n == -1) {
        perror("read");
        close(file_r);
        return 1;
    }

    int end = lseek(file_r, 0L, SEEK_CUR);

    if (end == -1) {
        perror("lseek");
        close(file_r);
        return 1;
    }

    table[line_count].len = end - line_start;
    line_count += 1;

    printf("\nresult:\n");
    printf("Line\nstart\tlen\n");

    for (int i = 0; i < line_count; i++) {
        printf("%d\t%d\t%d\n",
            i + 1, table[i].start, table[i].len);
    }

    signal(SIGALRM, finish);
    alarm(5);
    while (1) {
        char input[100];
        int line_number;


        printf("\ninput: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            break;
        }

        alarm(0);

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

        char *buffer = malloc(len + 1);

        lseek(file_r,
              table[line_number - 1].start,
              SEEK_SET);

        int bytes_read = read(file_r, buffer, len);

        if (bytes_read == -1) {
            perror("read");
            free(buffer);
            close(file_r);
            return 1;
        }

        buffer[bytes_read] = '\0';

        printf("%s", buffer);

        free(buffer);
        alarm(0);
    }

    close(file_r);

    return 0;
}
