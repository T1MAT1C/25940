#define _POSIX_C_SOURCE 200112L
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char *
editor_command(const char *editor, const char *path)
{
    size_t a = strlen(editor);
    size_t b = strlen(path);
    size_t prefix = path[0] == '/' ? 0 : 2;
    char *command;
    char *out;
    const char *p;

    if (a > SIZE_MAX - 8 || b > (SIZE_MAX - a - 8) / 4) {
        fprintf(stderr, "Editor command is too long\n");
        return NULL;
    }
    command = malloc(a + 4 * b + prefix + 4);
    if (command == NULL) {
        perror("malloc");
        return NULL;
    }
    memcpy(command, editor, a);
    out = command + a;
    *out++ = ' ';
    *out++ = '\'';
    if (prefix != 0) {
        *out++ = '.';
        *out++ = '/';
    }
    for (p = path; *p != '\0'; ++p) {
        if (*p == '\'') {
            memcpy(out, "'\\''", 4);
            out += 4;
        } else {
            *out++ = *p;
        }
    }
    *out++ = '\'';
    *out = '\0';
    return command;
}

int
main(int argc, char *argv[])
{
    struct flock lock;
    struct stat info;
    const char *editor = getenv("EDITOR");
    const char *path;
    char *command;
    int mandatory;
    int fd;
    int result;
    int status = EXIT_FAILURE;

    if (argc != 3 || (strcmp(argv[1], "advisory") != 0 &&
        strcmp(argv[1], "mandatory") != 0)) {
        fprintf(stderr, "Usage: %s advisory|mandatory file\n", argv[0]);
        return EXIT_FAILURE;
    }
    mandatory = strcmp(argv[1], "mandatory") == 0;
    path = argv[2];
    if (editor == NULL || *editor == '\0') {
        editor = "vi";
    }
    command = editor_command(editor, path);
    if (command == NULL) {
        return EXIT_FAILURE;
    }
    fd = open(path, O_RDWR);
    if (fd == -1) {
        perror(path);
        free(command);
        return EXIT_FAILURE;
    }
    if (fstat(fd, &info) == -1) {
        perror("fstat");
        goto cleanup;
    }
    if (!S_ISREG(info.st_mode)) {
        fprintf(stderr, "A regular file is required\n");
        goto cleanup;
    }
    if (mandatory != ((info.st_mode & S_ISGID) != 0 &&
        (info.st_mode & S_IXGRP) == 0)) {
        fprintf(stderr, "Mode mismatch: use chmod 0660 for advisory or "
            "chmod 2660 for mandatory locking\n");
        goto cleanup;
    }
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (printf("PID %lu: waiting for a whole-file %s lock\n",
        (unsigned long)getpid(), argv[1]) < 0 || fflush(stdout) == EOF) {
        perror("stdout");
        goto cleanup;
    }
    while (fcntl(fd, F_SETLKW, &lock) == -1) {
        if (errno != EINTR) {
            perror("fcntl lock");
            goto cleanup;
        }
    }
    if (printf("Lock acquired; running editor: %s\n", editor) < 0 ||
        fflush(stdout) == EOF) {
        perror("stdout");
        goto cleanup;
    }
    result = system(command);
    if (result == -1) {
        perror("system");
    } else if (WIFEXITED(result)) {
        if (printf("Editor exit status: %d\n", WEXITSTATUS(result)) >= 0) {
            status = WEXITSTATUS(result) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    } else if (WIFSIGNALED(result)) {
        fprintf(stderr, "Editor terminated by signal %d\n", WTERMSIG(result));
    }
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("fcntl unlock");
        status = EXIT_FAILURE;
    }

cleanup:
    if (close(fd) == -1) {
        perror("close");
        status = EXIT_FAILURE;
    }
    free(command);
    if (fflush(stdout) == EOF) {
        perror("fflush");
        status = EXIT_FAILURE;
    }
    return status;
}
