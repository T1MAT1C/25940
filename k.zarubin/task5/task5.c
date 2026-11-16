#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#define MAX_STRS 10000

off_t lens[MAX_STRS];
off_t offs[MAX_STRS];

int main(){
    int fd = open("file.txt", O_RDONLY);
    if (fd == -1){
        perror("open");
        return 1;
    }

    /* start - начало текущей строки */
    int found_strs = 0; off_t start = 0;
    char c;
    ssize_t r;

    /* читаем файл побайтово */
    while ((r = read(fd, &c, 1)) == 1){
        off_t pos = lseek(fd, 0L, SEEK_CUR);

        if (pos == (off_t)-1){
            perror("lseek");
            close(fd);
            return 1;
        }

        if (c == '\n'){
            if (found_strs >= MAX_STRS){
                fprintf(stderr, "Слишком много строк уже\n");
                close(fd);
                return 1;
            }

            offs[found_strs] = start;
            lens[found_strs] = pos - 1 - start;
            found_strs++;
            start = pos;
        }
    }
    /* проверяем ошибку чтения */
    if (r == -1){
        perror("read");
        close(fd);
        return 1;
    }

    /* выводим таблицу смещений */
    printf("\nТаблица смещений файла:\n\n");
    for (int i = 0; i < found_strs; i++){
        printf("Строка %d: смещение - %lld, длина - %lld\n", i + 1, (long long)offs[i], (long long)lens[i]);
    }
    printf("\n");

    long num;
    char input[128];

    printf("Привет! Вводи номер строки для просмотра строки содержимого файла (0 - завершение)\n");
    while (1){
        if (fgets(input, sizeof(input), stdin) == NULL) break;

        /* проверяем, поместилась ли вся строка */
        if (strchr(input, '\n') == NULL && !feof(stdin)){
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
            printf("Некорректный ввод\n");
            continue;
        }

        /* проверяем каждый символ на принадлежность к цифрам */
        int valid = 1;
        int i = 0;

        while (input[i] != '\0' && input[i] != '\n'){
            if (input[i] < '0' || input[i] > '9'){
                valid = 0;
                break;
            }
            i++;
        }

        if (!valid || i == 0){
            printf("Некорректный ввод\n");
            continue;
        }
        num = atol(input);
        
        if (num == 0){
            printf("Завершение работы\n");
            break;
        }

        if (num < 1 || num > found_strs){
            printf("Некорректный ввод (out of range)\n");
            continue;
        }

        int idx = (int)(num - 1);
        off_t off = offs[idx];
        off_t len = lens[idx];

        /* перемещаемся к началу выбранной строки */
        if (lseek(fd, off, SEEK_SET) == (off_t)-1){
            perror("lseek");
            close(fd);
            return 1;
        }

        char *buf = malloc((size_t)len + 1);
        if (buf == NULL){
            perror("malloc");
            close(fd);
            return 1;
        }

        /* читаем строку полностью */
        size_t total = 0;

        while (total < (size_t)len){
            ssize_t bytes_read = read(fd, buf + total, (size_t)len - total);

            if (bytes_read == -1){
                perror("read");
                free(buf);
                close(fd);
                return 1;
            }

            if (bytes_read == 0){
                fprintf(stderr, "Неожиданный конец файла\n");
                free(buf);
                close(fd);
                return 1;
            }

            total += (size_t)bytes_read;
        }

        buf[total] = '\0';
        printf("%s\n", buf);
        free(buf);
    }

    close(fd);
    return 0;
}
