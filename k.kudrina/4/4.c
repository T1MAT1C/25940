#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

#define MAXLEN 50000

typedef struct node{
    char line[MAXLEN];
    int length;
    struct node* next;
}node;

static size_t ansi_seq_len(const unsigned char *s) {
    if (s[0] != 0x1B) return 0;
    if (s[1] == '\0') return 1;

    if (s[1] == '[') {
        size_t i = 2;
        while (s[i] >= 0x20 && s[i] <= 0x3F) i++;
        if (s[i] >= 0x40 && s[i] <= 0x7E) return i + 1;
        return i;                                   
    }

    if (s[1] == ']') {
        size_t i = 2;
        while (s[i] && s[i] != 0x07) {
            if (s[i] == 0x1B && s[i+1] == '\\') return i + 2;
            i++;
        }
        return (s[i] == 0x07) ? i + 1 : i;
    }

    if (s[1] == '(' || s[1] == ')' || s[1] == '*' || s[1] == '+')
        return s[2] ? 3 : 2;
    return 2;
}

void strip_ansi(char *str) {
    unsigned char *src = (unsigned char *)str;
    unsigned char *dst = (unsigned char *)str;
    while (*src) {
        if (*src == 0x1B) {
            src += ansi_seq_len(src);
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}


void filter(char *s) {
    char *p = s;
    while (*s) {
        if (isprint((unsigned char)*s) || *s == ' ' || *s == '\t') {
            *p++ = *s;
        }
        s++;
    }
    *p = '\0';
}

int main(void){
    printf("> Вводите строки. Если вначале <.> - завершить\n");
    node* cur_node = malloc(sizeof(node));
    node* start = cur_node;
    while (1){
        fgets(cur_node->line, MAXLEN, stdin);
	cur_node->line[strcspn(cur_node->line, "\n")] = 0;
	strip_ansi(cur_node->line);
	filter(cur_node->line);
        if (cur_node->line[0]=='.') break;
        cur_node->length = strlen(cur_node->line) + 1;
        cur_node->next = malloc(sizeof(node));
        cur_node = cur_node->next;
    }

    printf("> Введённые строки: \n");
    cur_node = start;
    while (cur_node!=NULL) {
        printf("%s\n", cur_node->line);
	    cur_node = cur_node->next;
    }
}
