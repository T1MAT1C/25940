#include <stdio.h>      // printf, fgets, perror
#include <stdlib.h>     // malloc, free
#include <string.h>     // strlen, strcpy


struct Node
{
    char *text;
    struct Node *next;
};

/* Удаляем стрелочки из строки */
void remove_arrows(char *line)
{
    char *src = line;
    char *dst = line;

    while (*src != '\0')
    {
        /* Стрелки обычно имеют вид ESC [ A/B/C/D */
        if (*src == '\033' && src[1] == '[')
        {
            if (src[2] == 'A' || src[2] == 'B' ||
                src[2] == 'C' || src[2] == 'D')
            {
                src += 3;
                continue;
            }
        }

        *dst = *src;
        dst++;
        src++;
    }

    *dst = '\0';
}


int main()
{
    char line[1024];         // буфер
    struct Node *head = NULL;
    struct Node *tail = NULL;
    struct Node *node;
    struct Node *current;
    int len;

    while (1)
    {
        if (fgets(line, sizeof(line), stdin) == NULL) {break;}

        remove_arrows(line);

        if (line[0] == '.'){break;} // конец ввода

        len = strlen(line) + 1; // +'\0'

        node = malloc(sizeof(struct Node));
        if (node == NULL)
        {
            perror("malloc");
            return 1;
        }

        node->text = malloc(len);
        if (node->text == NULL)
        {
            perror("malloc");
            free(node);
            return 1;
        }
        strcpy(node->text, line);

        node->next = NULL;

        if (head == NULL)
        {
            head = node;
            tail = node;
        }
        else    // добавление в хвост
        {
            tail->next = node;
            tail = node;
        }
    }

    // вывод
    printf("\nстроки:\n");
    current = head;
    while (current != NULL)
    {
        printf("%s", current->text);
        current = current->next;
    }

    // free all
    current = head;
    while (current != NULL)
    {
        struct Node *next;
        next = current->next;
        free(current->text); // Освобождаем строку
        free(current);       // Освобождаем сам узел
        current = next;
    }

    return 0;
}
