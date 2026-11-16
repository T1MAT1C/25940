#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SIZE 4096

struct node {
    char *text;
    struct node *next;
};

static void
free_list(struct node *head)
{
    struct node *next;

    while (head != NULL) {
        next = head->next;
        free(head->text);
        free(head);
        head = next;
    }
}

static int
append_line(struct node **head, struct node **tail, const char *line)
{
    struct node *item;
    size_t length;

    length = strlen(line);
    item = malloc(sizeof(*item));
    if (item == NULL) {
        perror("malloc");
        return -1;
    }

    item->text = malloc(length + 1);
    if (item->text == NULL) {
        perror("malloc");
        free(item);
        return -1;
    }

    memcpy(item->text, line, length + 1);
    item->next = NULL;

    if (*tail == NULL) {
        *head = item;
    } else {
        (*tail)->next = item;
    }
    *tail = item;
    return 0;
}

static int
read_line(char *buffer, size_t capacity)
{
    size_t length;
    int character;

    if (fgets(buffer, (int)capacity, stdin) == NULL) {
        if (ferror(stdin)) {
            perror("fgets");
            return -1;
        }
        return 0;
    }

    length = strlen(buffer);
    if (length != 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
        return 1;
    }

    character = getchar();
    if (character == '\n' || character == EOF) {
        if (character == EOF && ferror(stdin)) {
            perror("getchar");
            return -1;
        }
        return 1;
    }

    while (character != '\n' && character != EOF) {
        character = getchar();
    }
    if (character == EOF && ferror(stdin)) {
        perror("getchar");
        return -1;
    }

    if (buffer[0] == '.') {
        return 1;
    }

    fprintf(stderr, "Input line is too long; line was ignored\n");
    return 2;
}

int
main(void)
{
    char buffer[INPUT_SIZE];
    struct node *head = NULL;
    struct node *tail = NULL;
    struct node *item;
    int result;
    int status = EXIT_SUCCESS;

    for (;;) {
        fputs("Enter a line (. to finish): ", stdout);
        if (fflush(stdout) == EOF) {
            perror("fflush");
            status = EXIT_FAILURE;
            break;
        }

        result = read_line(buffer, sizeof(buffer));
        if (result == -1) {
            status = EXIT_FAILURE;
            break;
        }
        if (result == 0) {
            break;
        }
        if (result == 2) {
            continue;
        }
        if (buffer[0] == '.') {
            break;
        }
        if (append_line(&head, &tail, buffer) == -1) {
            status = EXIT_FAILURE;
            break;
        }
    }

    if (status == EXIT_SUCCESS) {
        puts("Stored lines:");
        for (item = head; item != NULL; item = item->next) {
            if (puts(item->text) == EOF) {
                perror("puts");
                status = EXIT_FAILURE;
                break;
            }
        }
    }

    free_list(head);
    if (fflush(stdout) == EOF) {
        perror("fflush");
        status = EXIT_FAILURE;
    }
    return status;
}
