#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "shell.h"

#define MAX_TOKENS 512

enum token_kind { WORD, SEMICOLON, BACKGROUND, INPUT, OUTPUT, APPEND, PIPE };

struct token {
    enum token_kind kind;
    char *text;
};

static int
syntax(const char *message)
{
    fprintf(stderr, "Syntax error: %s\n", message);
    return -1;
}

int
parseline(char *buffer)
{
    struct token tokens[MAX_TOKENS];
    size_t count = 0;
    size_t i;
    size_t arguments = 0;
    char *src = buffer;
    char *dst = buffer;

    memset(cmds, 0, sizeof(cmds));
    command_count = 0;
    bkgrnd = 0;
    infile = NULL;
    outfile = NULL;
    appfile = 0;
    while (*src != '\0') {
        struct token *token;
        char c;

        while (isspace((unsigned char)*src)) {
            ++src;
        }
        if (*src == '\0') {
            break;
        }
        if (count == MAX_TOKENS) {
            return syntax("too many tokens");
        }
        token = &tokens[count++];
        token->text = NULL;
        c = *src;
        if (strchr(";&<>|", c) != NULL) {
            ++src;
            switch (c) {
            case ';': token->kind = SEMICOLON; break;
            case '&': token->kind = BACKGROUND; break;
            case '<': token->kind = INPUT; break;
            case '|': token->kind = PIPE; break;
            default:
                token->kind = OUTPUT;
                if (*src == '>') {
                    ++src;
                    token->kind = APPEND;
                }
                break;
            }
            continue;
        }
        token->kind = WORD;
        token->text = dst;
        while (*src != '\0' && !isspace((unsigned char)*src) &&
            strchr(";&<>|", *src) == NULL) {
            if (*src == '\'' || *src == '"') {
                char quote = *src++;

                while (*src != '\0' && *src != quote) {
                    if (quote == '"' && *src == '\\') {
                        ++src;
                        if (*src == '\0') {
                            return syntax("trailing escape");
                        }
                    }
                    *dst++ = *src++;
                }
                if (*src == '\0') {
                    return syntax("unclosed quote");
                }
                ++src;
            } else {
                if (*src == '\\') {
                    ++src;
                    if (*src == '\0') {
                        return syntax("trailing escape");
                    }
                }
                *dst++ = *src++;
            }
        }
        /* Save a delimiter before compacting over it with a null byte. */
        if (*src != '\0' && !isspace((unsigned char)*src)) {
            char delimiter = *src++;

            if (count == MAX_TOKENS) {
                return syntax("too many tokens");
            }
            tokens[count].text = NULL;
            switch (delimiter) {
            case ';': tokens[count].kind = SEMICOLON; break;
            case '&': tokens[count].kind = BACKGROUND; break;
            case '<': tokens[count].kind = INPUT; break;
            case '|': tokens[count].kind = PIPE; break;
            default:
                tokens[count].kind = OUTPUT;
                if (*src == '>') {
                    ++src;
                    tokens[count].kind = APPEND;
                }
                break;
            }
            ++count;
        } else if (*src != '\0') {
            ++src;
        }
        *dst++ = '\0';
    }
    if (count == 0) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        enum token_kind kind = tokens[i].kind;

        if (kind == WORD) {
            if (arguments == MAX_ARGUMENTS) {
                return syntax("too many arguments");
            }
            cmds[command_count].cmdargs[arguments++] = tokens[i].text;
        } else if (kind == INPUT || kind == OUTPUT || kind == APPEND) {
            if (++i == count || tokens[i].kind != WORD) {
                return syntax("redirection needs a filename");
            }
            if (kind == INPUT) {
                if (infile != NULL || command_count != 0) {
                    return syntax("input redirection belongs to the first command");
                }
                infile = tokens[i].text;
            } else {
                if (outfile != NULL) {
                    return syntax("duplicate output redirection");
                }
                outfile = tokens[i].text;
                appfile = kind == APPEND;
            }
        } else if (kind == PIPE) {
            return syntax("pipelines are introduced in lab29");
        } else if (kind == BACKGROUND) {
            if (i + 1 != count || arguments == 0) {
                return syntax("& must follow the last command");
            }
            bkgrnd = 1;
        } else {
            if (arguments == 0 || outfile != NULL) {
                return syntax("empty command or output before last command");
            }
            ++command_count;
            arguments = 0;
            if (i + 1 == count) {
                return 0;
            }
            if (command_count == MAX_COMMANDS) {
                return syntax("too many commands");
            }
        }
    }
    if (arguments == 0) {
        return syntax("missing command");
    }
    ++command_count;
    return 0;
}
