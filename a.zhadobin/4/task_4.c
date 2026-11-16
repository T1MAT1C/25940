
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
    char *str;
    struct node *next;
};


void print_help(void)
{
    printf("\n====================================\n");
    printf("           STRING LIST\n");
    printf("====================================\n");
    printf("Enter text lines to create a list.\n");
    printf("Each line is stored in memory.\n");
    printf("\nCommands:\n");
    printf("  help - show this information\n");
    printf("  .    - finish input and print list\n");
    printf("\nRules:\n");
    printf("  Maximum line length: 299 characters\n");
    printf("  Control characters are forbidden\n");
    printf("  Invalid input will be skipped\n");
    printf("====================================\n\n");
}

int check_input(const char *str)
{
    const unsigned char *p =
        (const unsigned char *)str;

    while (*p != '\0') {
        if (*p < 32 || *p == 127) {
            return 0;
        }
        p++;
    }

    return 1;
}

void free_list(struct node *start)
{
    struct node *current = start;

    while (current != NULL) {
        struct node *next = current->next;
        free(current->str);
        free(current);
        current = next;
    }
}

int main()
{
    char buffer[300];
    struct node *start = NULL;
    struct node *end = NULL;

    print_help();

    while (1) {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        int len = strlen(buffer);

        /* Remove newline if present */
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        } else if (len == sizeof(buffer) - 1) {
            /* Check whether the line is too long */
            int ch = getchar();

            if (ch != '\n' && ch != EOF) {
                while ((ch = getchar()) != '\n'
                       && ch != EOF) {
                }

                printf("Error: line is too long!\n");
                continue;
            }
        }

        if (!check_input(buffer)) {
            printf("Error: invalid characters!\n");
            printf("Please enter a valid line.\n");
            continue;
        }

        if (strcmp(buffer, "help") == 0) {
            print_help();
            continue;
        }

        struct node *new_node =
            malloc(sizeof(struct node));

        if (new_node == NULL) {
            perror("malloc node");
            free_list(start);
            return 1;
        }

        new_node->str = malloc(len + 1);

        if (new_node->str == NULL) {
            perror("malloc str");
            free(new_node);
            free_list(start);
            return 1;
        }

        strcpy(new_node->str, buffer);

        new_node->next = NULL;

        if (start == NULL) {
            start = new_node;
            end = new_node;
        } else {
            end->next = new_node;
            end = new_node;
        }

        if (buffer[0] == '.') {
            break;
        }
    }

    printf("\nresult\n");

    struct node *current = start;

    while (current != NULL) {
        printf("%s\n", current->str);
        current = current->next;
    }

    free_list(start);

    return 0;
}
