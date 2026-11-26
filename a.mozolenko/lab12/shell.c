#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "shell.h"

struct command cmds[MAX_COMMANDS];
size_t command_count;
int bkgrnd;
char *infile;
char *outfile;
int appfile;

int
main(void)
{
    char buffer[INPUT_SIZE];
    int input;

    if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Unable to disable stdin buffering\n");
        return EXIT_FAILURE;
    }
    /* First shell exercise: successful exec replaces the interpreter. */
    while ((input = prompline(buffer, sizeof(buffer))) > 0) {
        if (parseline(buffer) == -1 || command_count == 0) {
            continue;
        }
        if (command_count != 1 || bkgrnd || infile != NULL || outfile != NULL) {
            fprintf(stderr, "This stage executes one simple command\n");
            continue;
        }
        execvp(cmds[0].cmdargs[0], cmds[0].cmdargs);
        perror(cmds[0].cmdargs[0]);
    }
    return input < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
