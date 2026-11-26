
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>

static volatile sig_atomic_t sigint_count = 0;

// Обработчик SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    (void)sig;

    if (sigint_count < SIG_ATOMIC_MAX)
        sigint_count++;

    write(STDOUT_FILENO, "\a\n", 2);
}

// Обработчик SIGQUIT (Ctrl+\)
void handle_sigquit(int sig) {
    (void)sig;

    const char prefix[] = "\nSIGQUIT received. Total SIGINTs: ";
    char message[sizeof(prefix) + sizeof(uintmax_t) * CHAR_BIT];
    char digits[sizeof(uintmax_t) * CHAR_BIT];

    size_t pos = 0, count = 0;

    // Копируем сообщение
    for (size_t i = 0; i < sizeof(prefix) - 1; i++)
        message[pos++] = prefix[i];

    // Переводим количество сигналов в строку
    uintmax_t num = (uintmax_t)sigint_count;

    do {
        digits[count++] = '0' + num % 10;
        num /= 10;
    } while (num > 0);

    while (count > 0)
        message[pos++] = digits[--count];

    message[pos++] = '\n';

    write(STDOUT_FILENO, message, pos);
    _exit(0);
}

int main(void) {
    struct sigaction sa = {0};

    // Настраиваем маску сигналов
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);

    sa.sa_flags = 0;

    // Устанавливаем обработчик SIGINT
    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    // Устанавливаем обработчик SIGQUIT
    sa.sa_handler = handle_sigquit;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction SIGQUIT");
        return 1;
    }

    printf("Program started. Press Ctrl+C for beep, Ctrl+\\ to quit.\n");
    fflush(stdout);

    // Бесконечно ожидаем сигналы
    while (1)
        pause();

    return 0;
}