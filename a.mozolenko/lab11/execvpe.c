#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "execvpe.h"
extern char **environ;
static int
execute(const char *path, char *const argv[], char *const envp[])
{
    size_t count = 0;
    size_t i;
    char **shell_args;
    int error;
    execve(path, argv, envp);
    if (errno != ENOEXEC) {
        return -1;
    }
    while (argv[count] != NULL) {
        ++count;
    }
    if (count > SIZE_MAX / sizeof(*shell_args) - 3) {
        errno = E2BIG;
        return -1;
    }
    shell_args = malloc((count + 3) * sizeof(*shell_args));
    if (shell_args == NULL) {
        errno = ENOMEM;
        return -1;
    }
    shell_args[0] = (char *)"sh";
    shell_args[1] = (char *)path;
    for (i = 1; i < count; ++i) {
        shell_args[i + 1] = argv[i];
    }
    shell_args[count == 0 ? 2 : count + 1] = NULL;
    execve("/bin/sh", shell_args, envp);
    error = errno;
    free(shell_args);
    errno = error;
    return -1;
}
int
execvpe(const char *file, char *const argv[], char *const envp[])
{
    char **saved = environ;
    const char *path;
    const char *component;
    char *default_path = NULL;
    int error = ENOENT;
    int denied = 0;
    if (*file == '\0') {
        errno = ENOENT;
        return -1;
    }
    if (strchr(file, '/') != NULL) {
        return execute(file, argv, envp);
    }
    environ = (char **)envp;
    path = getenv("PATH");
    if (path == NULL) {
        size_t length = confstr(_CS_PATH, NULL, 0);
        if (length == 0) {
            error = errno == 0 ? EINVAL : errno;
            goto done;
        }
        default_path = malloc(length);
        if (default_path == NULL) {
            error = ENOMEM;
            goto done;
        }
        if (confstr(_CS_PATH, default_path, length) == 0) {
            error = errno;
            goto done;
        }
        path = default_path;
    }
    component = path;
    for (;;) {
        const char *end = strchr(component, ':');
        size_t length = end == NULL ? strlen(component) : (size_t)(end - component);
        size_t name_length = strlen(file);
        char *candidate;
        if (name_length > SIZE_MAX - 2 || length > SIZE_MAX - name_length - 2) {
            error = ENAMETOOLONG;
            break;
        }
        candidate = malloc(length + name_length + 2);
        if (candidate == NULL) {
            error = ENOMEM;
            break;
        }
        memcpy(candidate, component, length);
        if (length != 0) {
            candidate[length++] = '/';
        }
        memcpy(candidate + length, file, name_length + 1);
        execute(candidate, argv, envp);
        error = errno;
        free(candidate);
        if (error == EACCES) {
            denied = 1;
        } else if (error != ENOENT && error != ENOTDIR) {
            break;
        }
        if (end == NULL) {
            error = denied ? EACCES : ENOENT;
            break;
        }
        component = end + 1;
    }
done:
    free(default_path);
    environ = saved;
    errno = error;
    return -1;
}
