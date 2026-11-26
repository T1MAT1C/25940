#ifndef LAB_SHELL_H
#define LAB_SHELL_H

#include <stddef.h>

#define INPUT_SIZE 4096
#define MAX_COMMANDS 32
#define MAX_ARGUMENTS 128

struct command {
    char *cmdargs[MAX_ARGUMENTS + 1];
};

extern struct command cmds[MAX_COMMANDS];
extern size_t command_count;
extern int bkgrnd;
extern char *infile;
extern char *outfile;
extern int appfile;

int prompline(char *buffer, size_t capacity);
int parseline(char *buffer);

#endif
