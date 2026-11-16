
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

struct Line {
    int start;
    int len;
};

void print_help(void)
{
    printf("\n====================================\n");
    printf("          FILE LINE READER\n");
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
    printf("  Letters and special characters\n");
    printf("  are not allowed in line numbers\n");
    printf("  Invalid input will be skipped\n");
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

int main(int argc, char *argv[])
{
    int fd, n, line_start = 0;

    struct Line table[200];
    int line_count = 0;

    print_help();

    if (argc != 2) {
        fprintf(stderr,
                "Error: invalid arguments!\n");
        fprintf(stderr,
                "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    table[0].start = 0;

    char c;

    while ((n = read(fd, &c, 1)) > 0) {

        if (c == '\n') {
            int current_position = lseek(fd, 0, SEEK_CUR);

            if (current_position == -1) {
                perror("lseek");
                close(fd);
                return 1;
            }

            table[line_count].len =
                current_position - line_start;

            line_count += 1;
            line_start = current_position;

            if (line_count >= 200) {
                fprintf(stderr,
                    "Error: too many lines!\n");
                close(fd);
                return 1;
            }

            table[line_count].start = line_start;
        }
    }

    if (n == -1) {
        perror("read");
        close(fd);
        return 1;
    }

    int end = lseek(fd, 0L, SEEK_CUR);

    if (end == -1) {
        perror("lseek");
        close(fd);
        return 1;
    }

    table[line_count].len = end - line_start;
    line_count += 1;

    printf("\nresult:\n");
    printf("Line\nstart\tlen\n");

    for (int i = 0; i < line_count; i++) {
        printf("%d\t%d\t%d\n",
            i + 1,
            table[i].start,
            table[i].len);
    }

    while (1) {
        char input[100];
        int line_number;

        printf("\ninput: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        int len = strlen(input);

        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        } else if (len == sizeof(input) - 1) {
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

        if (errno == ERANGE ||
            number > INT_MAX) {
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
            printf("Valid range: 1 - %d\n", line_count);
            continue;
        }

        len = table[line_number - 1].len;

        char *buffer = malloc(len + 1);

        if (buffer == NULL) {
            perror("malloc");
            close(fd);
            return 1;
        }

        if (lseek(fd,
                  table[line_number - 1].start,
                  SEEK_SET) == -1) {
            perror("lseek");
            free(buffer);
            close(fd);
            return 1;
        }

        int bytes_read = read(fd, buffer, len);

        if (bytes_read == -1) {
            perror("read");
            free(buffer);
            close(fd);
            return 1;
        }

        buffer[bytes_read] = '\0';

        printf("%s", buffer);

        free(buffer);
    }

    close(fd);

    return 0;
}
