#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/stat.h>
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

    /* получаем размер файла */
    struct stat st;
    if (fstat(fd, &st) == -1){
        perror("fstat");
        close(fd);
        return 1;
    }

    off_t f_size = st.st_size;

    /* отображаем файл в память */
    char *file_buf = NULL;

    if (f_size > 0){
        file_buf = mmap(NULL, (size_t)f_size, PROT_READ, MAP_PRIVATE, fd, 0);

        if (file_buf == MAP_FAILED){
            perror("mmap");
            close(fd);
            return 1;
        }
    }

    /* start - начало текущей строки */
    int found_strs = 0;
    off_t start = 0;

    /* ищем строки в отображённой памяти */
    for (off_t i = 0; i < f_size; i++){
        if (file_buf[i] == '\n'){
            if (found_strs >= MAX_STRS){
                fprintf(stderr, "Слишком много строк уже\n");
                if (f_size > 0) munmap(file_buf, (size_t)f_size);
                close(fd);
                return 1;
            }
            offs[found_strs] = start;
            lens[found_strs] = i - start;
            found_strs++;
            start = i + 1;
        }
    }

    /* выводим таблицу смещений */
    printf("\nТаблица смещений файла:\n\n");
    for (int i = 0; i < found_strs; i++){
        printf("Строка %d: смещение - %lld, длина - %lld\n", i+1, (long long)offs[i], (long long)lens[i]);
    }
    printf("\n");

    long num;
    char input[128];
    int first_input = 1;

    printf("Привет! Вводи номер строки для просмотра строки содержимого файла (0 - завершение)\n");
    while (1){
        fflush(stdout);

        /* таймер работает только перед первым вводом */
        if (first_input){
            fd_set set;
            FD_ZERO(&set);
            FD_SET(STDIN_FILENO, &set);

            struct timeval timeout = {5, 0};
            int ready = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

            if (ready == 0){
                printf("\nВремя вышло! Содержимое файла:\n");
                if (f_size > 0 && fwrite(file_buf, 1, (size_t)f_size, stdout) != (size_t)f_size){
                    perror("fwrite");
                    munmap(file_buf, (size_t)f_size);
                    close(fd);
                    return 1;
                }

                if (f_size > 0) munmap(file_buf, (size_t)f_size);
                close(fd);
                return 0;
            }

            if (ready == -1){
                perror("select");
                if (f_size > 0) munmap(file_buf, (size_t)f_size);
                close(fd);
                return 1;
            }

            first_input = 0;
        }

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

        /* получаем смещение и длину выбранной строки */
        int idx = (int)(num - 1);
        off_t off = offs[idx];
        off_t len = lens[idx];

        /* выводим строку непосредственно из памяти */
        if (len > 0 && fwrite(file_buf + off, 1, (size_t)len, stdout) != (size_t)len){
            perror("fwrite");
            if (f_size > 0) munmap(file_buf, (size_t)f_size);
            close(fd);
            return 1;
        }
        putchar('\n');
    }
    
    /* освобождаем отображённую память */
    if (f_size > 0){
        if (munmap(file_buf, (size_t)f_size) == -1){
            perror("munmap");
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}
