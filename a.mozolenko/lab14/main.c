#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_SIZE 4096
#define MAX_ARGS 128

static void
child_changed(int signo)
{
    (void)signo;
}

static int
reap(void)
{
    for (;;) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);

        if (pid > 0) {
            continue;
        }
        if (pid == 0 || errno == ECHILD) {
            return 0;
        }
        if (errno != EINTR) {
            perror("waitpid background");
            return -1;
        }
    }
}

static int
parse(char *line, char **args, int *background)
{
    char *src = line;
    char *dst = line;
    size_t count = 0;

    *background = 0;
    while (*src != '\0') {
        while (isspace((unsigned char)*src)) {
            ++src;
        }
        if (*src == '\0') {
            break;
        }
        if (*src == '&') {
            ++src;
            while (isspace((unsigned char)*src)) {
                ++src;
            }
            if (*src != '\0' || count == 0) {
                goto syntax;
            }
            *background = 1;
            break;
        }
        if (count == MAX_ARGS) {
            goto syntax;
        }
        args[count++] = dst;
        while (*src != '\0' && !isspace((unsigned char)*src) && *src != '&') {
            if (strchr(";<>|", *src) != NULL) {
                goto syntax;
            }
            if (*src == '\'' || *src == '"') {
                char quote = *src++;

                while (*src != '\0' && *src != quote) {
                    if (quote == '"' && *src == '\\' && *++src == '\0') {
                        goto syntax;
                    }
                    *dst++ = *src++;
                }
                if (*src == '\0') {
                    goto syntax;
                }
                ++src;
            } else {
                if (*src == '\\' && *++src == '\0') {
                    goto syntax;
                }
                *dst++ = *src++;
            }
        }
        if (*src == '&') {
            ++src;
            while (isspace((unsigned char)*src)) {
                ++src;
            }
            if (*src != '\0') {
                goto syntax;
            }
            *background = 1;
        } else if (*src != '\0') {
            ++src;
        }
        *dst++ = '\0';
    }
    args[count] = NULL;
    return (int)count;

syntax:
    fprintf(stderr, "Syntax error: expected one command with optional final &\n");
    return -1;
}

/* SIGCHLD stays blocked outside pselect, including during foreground wait. */
static int
read_line(char *line, const sigset_t *wait_mask)
{
    size_t length = 0;
    int overflow = 0;

    if (isatty(STDIN_FILENO) &&
        (printf("lab14$ ") < 0 || fflush(stdout) == EOF)) {
        perror("prompt");
        return -1;
    }
    for (;;) {
        fd_set ready;
        int result;
        ssize_t bytes;
        char c;

        if (reap() == -1) {
            return -1;
        }
        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);
        result = pselect(STDIN_FILENO + 1, &ready, NULL, NULL, NULL, wait_mask);
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("pselect");
            return -1;
        }
        if (result == 0) {
            continue;
        }
        bytes = read(STDIN_FILENO, &c, 1);
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("stdin");
            return -1;
        }
        if (bytes == 0 || c == '\n') {
            if (overflow) {
                fprintf(stderr, "Input line is too long or contains null bytes\n");
                length = 0;
            }
            line[length] = '\0';
            return bytes == 0 && length == 0 && !overflow ? 0 : 1;
        }
        if (c == '\0' || length == INPUT_SIZE - 1) {
            overflow = 1;
        } else if (!overflow) {
            line[length++] = c;
        }
    }
}

int
main(void)
{
    struct sigaction action;
    sigset_t set;
    sigset_t old_mask;
    sigset_t wait_mask;
    char line[INPUT_SIZE];
    char *args[MAX_ARGS + 1];
    int last_status = 0;
    int input;

    memset(&action, 0, sizeof(action));
    action.sa_handler = child_changed;
    action.sa_flags = SA_NOCLDSTOP;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGCHLD, &action, NULL) == -1 ||
        sigemptyset(&set) == -1 || sigaddset(&set, SIGCHLD) == -1 ||
        sigprocmask(SIG_BLOCK, &set, &old_mask) == -1) {
        perror("SIGCHLD setup");
        return EXIT_FAILURE;
    }
    wait_mask = old_mask;
    if (sigdelset(&wait_mask, SIGCHLD) == -1) {
        perror("sigdelset");
        return EXIT_FAILURE;
    }
    while ((input = read_line(line, &wait_mask)) > 0) {
        int background;
        int count = parse(line, args, &background);
        pid_t child;
        int status;

        if (count <= 0) {
            if (count < 0) {
                last_status = 2;
            }
            continue;
        }
        if (count == 1 && !background && strcmp(args[0], "exit") == 0) {
            break;
        }
        if (fflush(NULL) == EOF) {
            perror("fflush");
            last_status = 1;
            break;
        }
        child = fork();
        if (child == -1) {
            perror("fork");
            last_status = 1;
            continue;
        }
        if (child == 0) {
            int error;

            if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
                perror("child signal mask");
                _exit(126);
            }
            execvp(args[0], args);
            error = errno;
            perror(args[0]);
            _exit(error == ENOENT ? 127 : 126);
        }
        if (background) {
            if (printf("Background PID: %lu\n", (unsigned long)child) < 0 ||
                fflush(stdout) == EOF) {
                perror("stdout");
                last_status = 1;
                break;
            }
            last_status = 0;
            continue;
        }
        while (waitpid(child, &status, 0) == -1) {
            if (errno != EINTR) {
                perror("waitpid");
                last_status = 1;
                goto done;
            }
        }
        last_status = WIFEXITED(status) ? WEXITSTATUS(status) :
            128 + WTERMSIG(status);
        if (last_status > 255) {
            last_status = 1;
        }
    }
    if (input < 0) {
        last_status = 1;
    }
done:
    if (reap() == -1 || sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        last_status = 1;
    }
    return last_status;
}
