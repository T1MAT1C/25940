#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t interrupted;

static void
handle_signal(int signo)
{
    interrupted = signo;
}

int
main(void)
{
    const int signals[] = { SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGTSTP };
    struct sigaction action;
    struct sigaction saved[sizeof(signals) / sizeof(signals[0])];
    struct termios original;
    struct termios raw;
    sigset_t blocked;
    sigset_t old_mask;
    sigset_t wait_mask;
    size_t installed = 0;
    size_t i;
    int status = EXIT_FAILURE;
    int changed = 0;
    unsigned char answer = 0;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Standard input must be a terminal\n");
        return EXIT_FAILURE;
    }
    if (tcgetattr(STDIN_FILENO, &original) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }
    if (sigemptyset(&blocked) == -1) {
        perror("sigemptyset");
        return EXIT_FAILURE;
    }
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        if (sigaddset(&blocked, signals[i]) == -1) {
            perror("sigaddset");
            return EXIT_FAILURE;
        }
    }
    if (sigprocmask(SIG_BLOCK, &blocked, &old_mask) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }
    wait_mask = old_mask;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    action.sa_mask = blocked;
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        if (sigdelset(&wait_mask, signals[i]) == -1 ||
            sigaction(signals[i], &action, &saved[i]) == -1) {
            perror("sigaction");
            goto cleanup;
        }
        ++installed;
    }
    raw = original;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ECHONL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (printf("Continue? [y/n]: ") < 0 || fflush(stdout) == EOF) {
        perror("stdout");
        goto cleanup;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) {
        perror("tcsetattr");
        goto cleanup;
    }
    changed = 1;
    for (;;) {
        fd_set ready;
        int result;
        ssize_t bytes;
        if (interrupted != 0) {
            break;
        }
        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);
        result = pselect(STDIN_FILENO + 1, &ready, NULL, NULL, NULL, &wait_mask);
        if (interrupted != 0) {
            break;
        }
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("pselect");
            break;
        }
        if (result == 0) {
            continue;
        }
        bytes = read(STDIN_FILENO, &answer, 1);
        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            break;
        }
        if (bytes == 0) {
            fprintf(stderr, "Terminal input ended\n");
            break;
        }
        status = EXIT_SUCCESS;
        break;
    }
cleanup:
    if (changed) {
        int result;
        do {
            result = tcsetattr(STDIN_FILENO, TCSANOW, &original);
        } while (result == -1 && errno == EINTR);
        if (result == -1) {
            perror("restore terminal");
            status = EXIT_FAILURE;
        }
    }
    for (i = 0; i < installed; ++i) {
        if (sigaction(signals[i], &saved[i], NULL) == -1) {
            perror("restore sigaction");
            status = EXIT_FAILURE;
        }
    }
    if (interrupted != 0 && raise((int)interrupted) != 0) {
        perror("raise");
        status = EXIT_FAILURE;
    }
    if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        perror("restore signal mask");
        return EXIT_FAILURE;
    }
    if (interrupted == 0 && status == EXIT_SUCCESS) {
        if (printf("\nAnswer: 0x%02X", (unsigned int)answer) < 0 ||
            (answer >= 32 && answer <= 126 && printf(" ('%c')", answer) < 0) ||
            putchar('\n') == EOF || fflush(stdout) == EOF) {
            perror("stdout");
            return EXIT_FAILURE;
        }
    }
    return status;
}
