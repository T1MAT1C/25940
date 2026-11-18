
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

// Проверяем существование и доступность файла
int file_exists(char *filename) {
    struct stat st;
    return stat(filename, &st) == 0 &&
           S_ISREG(st.st_mode) && access(filename, R_OK) == 0;
}

// Получаем имя файла
char *get_filename(int argc, char *argv[]) {
    char *filename = NULL;
    size_t size = 0;

    if (argc > 2) {
        printf("Использование: %s [файл]\n", argv[0]);
        return NULL;
    }

    if (argc == 2) {
        filename = strdup(argv[1]);
        if (!filename) {
            perror("strdup");
            return NULL;
        }
        size = strlen(filename) + 1;
    }

    while (!filename || !filename[0] || !file_exists(filename)) {
        if (filename && filename[0])
            printf("Файл не найден или недоступен!\n");

        printf("Введите имя файла: ");
        fflush(stdout);

        if (getline(&filename, &size, stdin) == -1) {
            free(filename);
            return NULL;
        }

        filename[strcspn(filename, "\n")] = '\0';
    }

    return filename;
}

// Создаём дочерний процесс
pid_t start_child(char *filename) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        perror("execlp");
        _exit(127);
    }

    return pid;
}

// Ожидаем завершения дочернего процесса
int wait_child(pid_t pid) {
    int status;

    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        printf("Дочерний процесс завершился с ошибкой.\n");

    return 0;
}

int main(int argc, char *argv[]) {
    char *filename = get_filename(argc, argv);

    if (!filename)
        return 1;

    printf("\nПараллельное выполнение\n");
    fflush(stdout);

    pid_t pid = start_child(filename);
    if (pid == -1) {
        free(filename);
        return 1;
    }

    printf("Родитель: начал работу\n");
    printf("Родитель: закончил работу\n");
    fflush(stdout);

    wait_child(pid);

    printf("\nОжидание завершения\n");
    fflush(stdout);

    pid = start_child(filename);
    if (pid == -1) {
        free(filename);
        return 1;
    }

    printf("Родитель: начал работу\n");
    fflush(stdout);

    wait_child(pid);

    printf("модифицируйте программу так, чтобы последняя строка, распечатанная родителем, выводилась после завершения порожденного процесса.\n");

    free(filename);
    return 0;
}