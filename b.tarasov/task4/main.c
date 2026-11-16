#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct node {
    char *text;
    struct node *next;
};

static void free_list(struct node *head)
{
    while (head) {
        struct node *next = head->next;
        free(head->text);
        free(head);
        head = next;
    }
}

/* Reads a complete line, even if it exceeds the fgets buffer. */
static int read_line(char **out)
{
    char chunk[1024];
    char *line = NULL;
    size_t used = 0;

    while (fgets(chunk, sizeof(chunk), stdin)) {
        size_t length = strlen(chunk);
        char *new_line;

        if (length > SIZE_MAX - used - 1) {
            free(line);
            return -1;
        }

        new_line = realloc(line, used + length + 1);
        if (!new_line) {
            perror("realloc");
            free(line);
            return -1;
        }

        line = new_line;
        memcpy(line + used, chunk, length + 1);
        used += length;

        if (used && line[used - 1] == '\n') {
            *out = line;
            return 1;
        }
    }

    if (ferror(stdin)) {
        perror("stdin");
        free(line);
        return -1;
    }

    *out = line;
    return used ? 1 : 0;
}

int main(void)
{
    struct node *head = NULL;
    struct node **tail = &head;
    struct node *p;
    char *line;
    int result;

    while ((result = read_line(&line)) == 1) {
        if (line[0] == '.') {
            free(line);
            break;
        }

        p = malloc(sizeof(*p));
        if (!p) {
            perror("malloc");
            free(line);
            free_list(head);
            return 1;
        }

        p->text = line;
        p->next = NULL;

        *tail = p;
        tail = &p->next;
    }

    if (result < 0) {
        free_list(head);
        return 1;
    }

    for (p = head; p; p = p->next) {
        if (fputs(p->text, stdout) == EOF) {
            perror("stdout");
            free_list(head);
            return 1;
        }
    }

    free_list(head);
    return 0;
}
