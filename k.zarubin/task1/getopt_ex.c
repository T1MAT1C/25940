#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>
#include <ulimit.h>

#define OPT_STR "ispuU:cC:dvV:"
#define MAX_OPTS 100
#define MAX_ARGS 1024

/* Структура для хранения опции и ее аргумента */
typedef struct {
  int opt_name;
  char *arg;
} Option;

/* Проверка, является ли символ допустимой опцией */
int is_option_char(char c) { return c != '\0' && strchr("ispuUcCdvV", c) != NULL; }
/* Проверка, требует ли опция аргумент */
int takes_argument(char c) { return c == 'U' || c == 'C' || c == 'V'; }
/* Добавление аргумента в массив */
int add_argument(char **args, int *count, const char *value, size_t len) {
  if (*count >= MAX_ARGS - 1) {
    fprintf(stderr, "too many arguments\n");
    return -1;
  }

  args[*count] = malloc(len + 1);
  if (args[*count] == NULL) {
    perror("malloc");
    return -1;
  }

  memcpy(args[*count], value, len);
  args[*count][len] = '\0';
  (*count)++;

  return 0;
}

/* Разделение слитно записанных опций, например -i-p */
int expand_arguments(int argc, char **argv, char **result) {
  int count = 1;
  result[0] = argv[0];

  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];

    /* Передача обычного аргумента без изменений */
    if (arg[0] != '-' || arg[1] == '\0' || arg[1] == '-' || strchr(arg + 1, '-') == NULL) {
      if (add_argument(result, &count, arg, strlen(arg)) == -1) goto error;
      continue;
    }

    int j = 1;

    while (arg[j] != '\0') {
      if (arg[j] == '-') {
        j++;
        continue;
      }

      char option = arg[j++];

      /* Передача неизвестной опции для обработки ошибки */
      if (!is_option_char(option)) {
        char token[] = {'-', option, '\0'};

        if (add_argument(result, &count, token, 2) == -1) goto error;
        continue;
      }

      /* Обработка опций, принимающих аргумент */
      if (takes_argument(option)) {
        int start = j;
        int end = j;

        while (arg[end] != '\0' && !(arg[end] == '-' && is_option_char(arg[end + 1]))) { end++; }
        char *token = malloc((size_t)(end - start) + 3);
        if (token == NULL) {
          perror("malloc");
          goto error;
        }

        token[0] = '-';
        token[1] = option;
        memcpy(token + 2, arg + start, (size_t)(end - start));
        token[end - start + 2] = '\0';

        int status = add_argument(result, &count, token, strlen(token));
        free(token);

        if (status == -1) goto error;
        j = end;
      } else {
        /* Разделение опции, не принимающей аргумент */
        char token[] = {'-', option, '\0'};
        if (add_argument(result, &count, token, 2) == -1) goto error;
      }
    }
  }

  result[count] = NULL;
  return count;

error:
  /* Освобождение выделенной памяти при ошибке */
  for (int i = 1; i < count; i++) { free(result[i]); }
  return -1;
}

/* Освобождение памяти аргументов */
void free_arguments(char **args, int count) {
  for (int i = 1; i < count; i++) { free(args[i]); }
}
/* ---------------------- */

int main(int argc, char *argv[]) {
  Option options[MAX_OPTS];
  char *expanded_argv[MAX_ARGS];

  /* Подготовка аргументов для getopt */
  int expanded_argc = expand_arguments(argc, argv, expanded_argv);
  if (expanded_argc == -1) return 1;

  int cnt = 0;
  int c;
  opterr = 0;

  /* Разбор опций и сохранение их в массив */
  while ((c = getopt(expanded_argc, expanded_argv, OPT_STR)) != -1) {
    if (cnt >= MAX_OPTS) {
      fprintf(stderr, "too many arguments\n");
      free_arguments(expanded_argv, expanded_argc);
      return 1;
    }

    if (c == '?') options[cnt].opt_name = optopt;
    else options[cnt].opt_name = c;

    options[cnt].arg = optarg;
    cnt++;
  }

  /* Проверка наличия опций */
  if (cnt == 0) {
    fprintf(stderr, "No options provided\n");
    free_arguments(expanded_argv, expanded_argc);
    return 1;
  }

  /* Проверка лишних аргументов до выполнения опций */
  if (optind < expanded_argc) {
    fprintf(stderr, "Unexpected argument: %s\n", expanded_argv[optind]);
    free_arguments(expanded_argv, expanded_argc);
    return 1;
  }

  int opt = 1;

  /* Выполнение опций в обратном порядке */
  for (int i = cnt - 1; i >= 0; i--) {
    printf("\n");
    if (cnt > 1) printf("%d.\n", opt);

    switch (options[i].opt_name) {
      case 'i': {
        /* Вывод реальных и эффективных UID и GID */
        printf("UID Real: %ld\n", (long)getuid());
        printf("UID Effective: %ld\n", (long)geteuid());
        printf("GID Real: %ld\n", (long)getgid());
        printf("GID Effective: %ld\n\n", (long)getegid());
        break;
      }

      case 's': {
        /* Проверка, является ли процесс лидером группы */
        if (getpid() == getpgrp()) {
          printf("Process is already a group leader: PID = PGID (%ld, %ld)\n\n", (long)getpid(), (long)getpgrp());
          break;
        }

        /* Создание новой группы процессов */
        if (setpgid(0, 0) == -1) {
          perror("setpgid");
          break;
        }

        /* Проверка результата */
        if (getpid() == getpgrp()) printf("Process successfully became a group leader! PID = PGID (%ld, %ld)\n\n", (long)getpid(), (long)getpgrp());
        else printf("Error: process is not a group leader:\nPID != PGID (%ld, %ld)\n\n", (long)getpid(), (long)getpgrp());
        break;
      }

      case 'p': {
        /* Вывод PID, PPID и PGID */
        printf("PID: %ld\n", (long)getpid());
        printf("PPID: %ld\n", (long)getppid());
        printf("PGID: %ld\n\n", (long)getpgrp());
        break;
      }

      case 'u': {
        /* Вывод текущего ограничения размера файла */
        errno = 0;
        long limit = ulimit(UL_GETFSIZE, 0);

        if (limit == -1 && errno != 0) {
          perror("ulimit");
          break;
        }

        printf("Ulimit: %ld\n\n", limit);
        break;
      }

      case 'U': {
        /* Проверка наличия аргумента */
        if (options[i].arg == NULL) {
          fprintf(stderr, "Option -U requires an argument\n\n");
          break;
        }

        /* Проверка значения ограничения */
        char *end;
        errno = 0;
        long value = strtol(options[i].arg, &end, 10);

        if (errno != 0 || end == options[i].arg || *end != '\0' || options[i].arg[0] == '-' || value < 0) {
          fprintf(stderr, "Invalid ulimit value\n\n");
          break;
        }

        /* Изменение ограничения размера файла */
        errno = 0;
        long result = ulimit(UL_SETFSIZE, value);

        if (result == -1 && errno != 0) {
          perror("ulimit");
          break;
        }

        printf("Ulimit changed to %ld\n\n", value);
        break;
      }

      case 'c': {
        /* Получение ограничения размера core-файла */
        struct rlimit limit;

        if (getrlimit(RLIMIT_CORE, &limit) == -1) {
          perror("getrlimit");
          break;
        }

        if (limit.rlim_cur == RLIM_INFINITY) {
          printf("Core file limit is unlimited\n\n");
        } else {
          printf("Core file limit: %llu\n\n",
            (unsigned long long)limit.rlim_cur);
        }
        break;
      }

      case 'C': {
        /* Проверка наличия аргумента */
        if (options[i].arg == NULL) {
          fprintf(stderr, "Option -C requires an argument\n\n");
          break;
        }

        /* Проверка значения ограничения */
        unsigned long long value;
        char *end;

        errno = 0;
        value = strtoull(options[i].arg, &end, 10);

        if (errno != 0 || end == options[i].arg || *end != '\0' || options[i].arg[0] == '-') {
          fprintf(stderr, "Invalid core file limit\n\n");
          break;
        }

        struct rlimit limit;

        /* Получение текущих ограничений */
        if (getrlimit(RLIMIT_CORE, &limit) == -1) {
          perror("getrlimit");
          break;
        }

        /* Проверка допустимости значения */
        if ((unsigned long long)(rlim_t)value != value) {
          fprintf(stderr, "Core file limit is too large\n\n");
          break;
        }

        /* Установка нового ограничения */
        limit.rlim_cur = (rlim_t)value;

        if (setrlimit(RLIMIT_CORE, &limit) == -1) {
          perror("setrlimit");
          break;
        }

        printf("Core file limit changed to %llu\n\n", value);
        break;
      }

      case 'd': {
        /* Вывод текущего рабочего каталога */
        char cwd[4096];
      
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
          perror("getcwd");
          break;
        }
      
        printf("Current directory: %s\n\n", cwd);
        break;
      }

      case 'v': {
        /* Вывод переменных окружения */
        extern char **environ;

        for (char **env = environ; *env != NULL; env++) { printf("%s\n", *env); }
        printf("\n");
        break;
      }

      case 'V': {
        /* Проверка наличия аргумента */
        if (options[i].arg == NULL) {
          fprintf(stderr, "Option -V requires an argument\n\n");
          break;
        }

        /* Проверка формата name=value */
        char *equal = strchr(options[i].arg, '=');

        if (equal == NULL || equal == options[i].arg) {
          fprintf(stderr, "Invalid environment variable format. Use name=value\n\n");
          break;
        }

        /* Установка переменной окружения */
        *equal = '\0';

        if (setenv(options[i].arg, equal + 1, 1) == -1) {
          perror("setenv");
          *equal = '=';
          break;
        }

        *equal = '=';

        printf("Environment variable set: %s\n\n", options[i].arg);
        break;
      }

      default: {
        /* Обработка неизвестной опции */
        fprintf(stderr, "Unknown option - %c\n\n", options[i].opt_name);
        break;
      }
    }
    opt++;
  }
  free_arguments(expanded_argv, expanded_argc);
  return 0;
}
