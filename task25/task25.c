#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Функция для дочернего процесса: читает из канала и выводит в верхнем регистре
void child_process(int read_fd) {
    // Закрываем дескриптор записи, так как нам нужно только чтение
    close(read_fd + 1); 

    char buffer[256];
    ssize_t bytes_read;

    printf("Дочерний процесс запущен (PID: %d)\n", getpid());

    // Читаем данные блоками из канала
    while ((bytes_read = read(read_fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; ++i) {
            // Преобразуем каждый символ в верхний регистр и выводим
            putchar(toupper((unsigned char)buffer[i]));
        }
        fflush(stdout); // Принудительная сброска буфера для немедленного вывода
    }

    if (bytes_read < 0) {
        perror("Ошибка чтения из канала");
    }

    close(read_fd);
    _exit(EXIT_SUCCESS);
}

// Функция для родительского процесса: читает с клавиатуры и пишет в канал
void parent_process(int write_fd, pid_t child_pid) {
    // Закрываем дескриптор чтения, так как нам нужно только запись
    close(write_fd - 1); 

    char input_buffer[256];
    ssize_t bytes_read;

    printf("Родительский процесс запущен (PID: %d). Введите текст (Ctrl+D для завершения):\n", getpid());

    // Цикл чтения с клавиатуры и записи в канал
    while ((bytes_read = read(STDIN_FILENO, input_buffer, sizeof(input_buffer))) > 0) {
        if (write(write_fd, input_buffer, bytes_read) != bytes_read) {
            perror("Ошибка записи в канал");
            break;
        }
    }

    // Критически важно: закрыть дескриптор записи.
    // Это сигнализирует дочернему процессу о том, что больше данных не будет (EOF).
    close(write_fd);

    // Ждем завершения дочернего процесса и получаем его код возврата
    int status;
    if (waitpid(child_pid, &status, 0) == -1) {
        perror("Ошибка ожидания дочернего процесса");
    } else {
        printf("Дочерний процесс завершился с кодом: %d\n", WEXITSTATUS(status));
    }
}

int main(void) {
    int pipe_fds[2];
    pid_t child_pid;

    // Создание канала для межпроцессного взаимодействия
    if (pipe(pipe_fds) == -1) {
        perror("Не удалось создать канал");
        return EXIT_FAILURE;
    }

    // Разделение процесса
    child_pid = fork();
    
    if (child_pid < 0) {
        perror("Ошибка создания дочернего процесса");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return EXIT_FAILURE;
    }

    if (child_pid == 0) {
        // Дочерний процесс
        child_process(pipe_fds[0]);
    } else {
        // Родительский процесс
        parent_process(pipe_fds[1], child_pid);
    }

    return EXIT_SUCCESS;
}