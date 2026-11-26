#define _POSIX_C_SOURCE 200112L
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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
#define MAX_COMMANDS 32
#define MAX_TOKENS 512

enum kind { WORD, END, SEMICOLON, AMPERSAND, INPUT, OUTPUT, APPEND, PIPE };
struct token { enum kind kind; char *text; };
struct command { char *args[MAX_ARGS + 1]; size_t count; };
struct job {
    struct command commands[MAX_COMMANDS];
    size_t count;
    char *infile;
    char *outfile;
    int append;
    int background;
};

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

static enum kind
operator_kind(char c, char **source)
{
    switch (c) {
    case ';': return SEMICOLON;
    case '&': return AMPERSAND;
    case '<': return INPUT;
    case '|': return PIPE;
    default:
        if (**source == '>') {
            ++*source;
            return APPEND;
        }
        return OUTPUT;
    }
}

static int
parse(char *line, struct job *job)
{
    struct token tokens[MAX_TOKENS + 1];
    size_t count = 0;
    size_t i;
    char *src = line;
    char *dst = line;
    memset(job, 0, sizeof(*job));
    while (*src != '\0') {
        while (isspace((unsigned char)*src)) {
            ++src;
        }
        if (*src == '\0') {
            break;
        }
        if (count == MAX_TOKENS) {
            goto syntax;
        }
        if (strchr(";&<>|", *src) != NULL) {
            char c = *src++;
            tokens[count].text = NULL;
            tokens[count++].kind = operator_kind(c, &src);
            continue;
        }
        tokens[count].kind = WORD;
        tokens[count++].text = dst;
        while (*src != '\0' && !isspace((unsigned char)*src) &&
            strchr(";&<>|", *src) == NULL) {
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
        if (*src != '\0' && !isspace((unsigned char)*src)) {
            char c = *src++;
            if (count == MAX_TOKENS) {
                goto syntax;
            }
            tokens[count].text = NULL;
            tokens[count++].kind = operator_kind(c, &src);
        } else if (*src != '\0') {
            ++src;
        }
        *dst++ = '\0';
    }
    tokens[count].kind = END;
    for (i = 0; i < count; ++i) {
        struct command *cmd = &job->commands[job->count];
        enum kind kind = tokens[i].kind;
        if (kind == WORD) {
            if (cmd->count == MAX_ARGS) {
                goto syntax;
            }
            cmd->args[cmd->count++] = tokens[i].text;
        } else if (kind == INPUT || kind == OUTPUT || kind == APPEND) {
            if (tokens[++i].kind != WORD) {
                goto syntax;
            }
            if (kind == INPUT) {
                if (job->count != 0 || job->infile != NULL) {
                    goto syntax;
                }
                job->infile = tokens[i].text;
            } else {
                if (job->outfile != NULL) {
                    goto syntax;
                }
                job->outfile = tokens[i].text;
                job->append = kind == APPEND;
            }
        } else if (kind == SEMICOLON) {
            if (cmd->count == 0 || job->outfile != NULL) {
                goto syntax;
            }
            ++job->count;
            if (i + 1 == count) {
                return 0;
            }
            if (job->count == MAX_COMMANDS) {
                goto syntax;
            }
        } else if (kind == AMPERSAND) {
            if (i + 1 != count || cmd->count == 0) {
                goto syntax;
            }
            job->background = 1;
        } else {
            goto syntax;
        }
    }
    if (count != 0) {
        if (job->commands[job->count].count == 0) {
            goto syntax;
        }
        ++job->count;
    }
    return 0;
syntax:
    fprintf(stderr, "Syntax error: invalid command, limit, quote or redirection\n");
    return -1;
}

static int
read_line(char *line, const sigset_t *wait_mask)
{
    size_t length = 0;
    int invalid = 0;
    if (isatty(STDIN_FILENO) &&
        (printf("lab15$ ") < 0 || fflush(stdout) == EOF)) {
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
            if (invalid) {
                fprintf(stderr, "Input line is too long or contains null bytes\n");
                length = 0;
            }
            line[length] = '\0';
            return bytes == 0 && length == 0 && !invalid ? 0 : 1;
        }
        if (c == '\0' || length == INPUT_SIZE - 1) {
            invalid = 1;
        } else if (!invalid) {
            line[length++] = c;
        }
    }
}

static int
redirect(const char *path, int target, int flags)
{
    int fd = open(path, flags, 0666);
    int replacement;
    if (fd == -1) {
        perror(path);
        return -1;
    }
    if (fd == target) {
        return 0;
    }
    if (close(target) == -1 && errno != EBADF) {
        perror("close standard descriptor");
        if (close(fd) == -1) {
            perror("close redirection");
        }
        return -1;
    }
    replacement = dup(fd);
    if (replacement != target) {
        perror("dup");
        if (replacement != -1 && close(replacement) == -1) {
            perror("close duplicate");
        }
        if (close(fd) == -1) {
            perror("close redirection");
        }
        return -1;
    }
    if (close(fd) == -1) {
        perror("close redirection");
        return -1;
    }
    return 0;
}

static int
execute(const struct job *job, const sigset_t *old_mask)
{
    size_t i;
    int last_status = 0;
    for (i = 0; i < job->count; ++i) {
        pid_t child;
        int status;
        if (fflush(NULL) == EOF) {
            perror("fflush");
            return 1;
        }
        child = fork();
        if (child == -1) {
            perror("fork");
            return 1;
        }
        if (child == 0) {
            int error;
            if (sigprocmask(SIG_SETMASK, old_mask, NULL) == -1) {
                perror("child signal mask");
                _exit(126);
            }
            if (i == 0 && job->infile != NULL &&
                redirect(job->infile, STDIN_FILENO, O_RDONLY) == -1) {
                _exit(126);
            }
            if (i + 1 == job->count && job->outfile != NULL &&
                redirect(job->outfile, STDOUT_FILENO, O_WRONLY | O_CREAT |
                (job->append ? O_APPEND : O_TRUNC)) == -1) {
                _exit(126);
            }
            execvp(job->commands[i].args[0], job->commands[i].args);
            error = errno;
            perror(job->commands[i].args[0]);
            _exit(error == ENOENT ? 127 : 126);
        }
        if (job->background) {
            if (printf("Background PID: %lu\n", (unsigned long)child) < 0 ||
                fflush(stdout) == EOF) {
                perror("stdout");
                return 1;
            }
            continue;
        }
        while (waitpid(child, &status, 0) == -1) {
            if (errno != EINTR) {
                perror("waitpid");
                return 1;
            }
        }
        last_status = WIFEXITED(status) ? WEXITSTATUS(status) :
            128 + WTERMSIG(status);
        if (last_status > 255) {
            last_status = 1;
        }
    }
    return last_status;
}

int
main(void)
{
    struct sigaction action;
    sigset_t set;
    sigset_t old_mask;
    sigset_t wait_mask;
    char line[INPUT_SIZE];
    struct job job;
    int input;
    int last_status = 0;
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
        if (parse(line, &job) == -1) {
            last_status = 2;
            continue;
        }
        if (job.count == 0) {
            continue;
        }
        if (job.count == 1 && job.commands[0].count == 1 &&
            !job.background && job.infile == NULL && job.outfile == NULL &&
            strcmp(job.commands[0].args[0], "exit") == 0) {
            break;
        }
        last_status = execute(&job, &old_mask);
    }
    if (input < 0) {
        last_status = 1;
    }
    if (reap() == -1 || sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        last_status = 1;
    }
    return last_status;
}
