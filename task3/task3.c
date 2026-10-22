#include <stdio.h>      // printf(), fopen(), fclose(), perror(), FILE
#include <unistd.h>     // getuid(), geteuid(), setuid()


// Функция пытается открыть файл и сообщает результат
void check_file(const char *filename)
{
    FILE *file;                         // Указатель на файл

    file = fopen(filename, "r");        // Пытаемся открыть файл только для чтения

    if (file == NULL)                   // Если открыть файл не получилось
    {
        perror("fopen");                // Печатаем причину ошибки
    }
    else                                // Если файл успешно открылся
    {
        printf("Файл открыт успешно\n");// Сообщаем об успехе
        fclose(file);                   // Закрываем файл
    }
}


int main()
{
    printf("До setuid:\n");             // Заголовок перед первой проверкой

    printf("UID = %ld, EUID = %ld\n",   // Печатаем реальный и эффективный UID
           (long)getuid(),              // Реальный UID пользователя
           (long)geteuid());            // Эффективный UID процесса

    check_file("mibombo.txt");          // Первая попытка открыть файл


    if (setuid(getuid()) == -1)         // Делаем EUID равным реальному UID
    {
        perror("setuid");               // Если setuid не сработал — выводим ошибку
        return 1;                       // Завершаем программу с ошибкой
    }


    printf("\nПосле setuid:\n");        // Заголовок перед второй проверкой

    printf("UID = %ld, EUID = %ld\n",   // Снова печатаем UID и EUID
           (long)getuid(),              // Реальный UID
           (long)geteuid());            // Эффективный UID

    check_file("my_file.txt");          // Вторая попытка открыть тот же файл


    return 0;                           // Программа успешно завершилась
}