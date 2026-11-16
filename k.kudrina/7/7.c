#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_LINE_COUNT 1024

unsigned int all_line_sizes[MAX_LINE_COUNT] = {0};
unsigned int all_line_offsets[MAX_LINE_COUNT];
unsigned int line_counter = 0;
char* map;

void handle_timeout(int sig) {
    for (int i = 0; i < line_counter; ++i){
        int start_i = all_line_offsets[i];
        int size_i = all_line_sizes[i];
        char* line = calloc(size_i + 1, sizeof(char));
        memcpy(line, map + start_i, size_i);
        line[size_i] = '\0';
        printf("Line %d: %s\n", i + 1, line);
    }   
    exit(0);
}

int main()
{
    char c;
    int in = open("input.txt", 0);

    struct stat sb;
    if (fstat(in, &sb) == -1) {
        perror("Fstat failed");
        close(in);
        return 1;
    }
    size_t file_size = sb.st_size;
    if (file_size == 0) {
        perror("File is empty");
        close(in);
        return 0;
    }
    map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, in, 0);
    if (map == MAP_FAILED) {
        perror("Mmap failed");
        close(in);
        return 1;
    }
    close(in);
    signal(SIGALRM, handle_timeout);

    for (int i = 0; i < file_size; ++i){
        if (line_counter < MAX_LINE_COUNT){
            all_line_offsets[line_counter] = line_counter == 0 ? SEEK_SET : all_line_offsets[line_counter - 1] + all_line_sizes[line_counter - 1];
            while (map[i] != '\n' && map[i] != '\0' && map[i] != EOF) {
                all_line_sizes[line_counter]++;
                i++;
                if (i>=file_size) break;
            }
            all_line_sizes[line_counter]++;
            line_counter++;
        }
        else{
            break;
        }
    }
    
    int R;
    while(1){
        printf("> Point number of line (you have 5 sec):\n");
        alarm(5);
        if (scanf("%d", &R)==1 && R >=0 && R <= line_counter) alarm(0);
        else{
            printf("Wrong line number\n");
	    while ((c = getchar())!='\n' && c != EOF){}
	    continue;
        } 
        if (R == 0) return 0;

        int start_R = all_line_offsets[R - 1];
        int size_R = all_line_sizes[R - 1];
        char* line = calloc(all_line_sizes[R - 1] + 1, sizeof(char));
        memcpy(line, map + start_R, size_R);
        line[size_R] = '\0';
        printf("Line %d: %s\n", R, line);
    }
    close(in);
    return 0;
}
