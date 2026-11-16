#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LINE_COUNT 1024

unsigned int all_line_sizes[MAX_LINE_COUNT] = {0};
unsigned int all_line_offsets[MAX_LINE_COUNT];
unsigned int line_counter = 0;
int in;

int main()
{
    char c;
    in = open("input.txt", 0);
    while (read(in, &c, 1)!=0 && line_counter < MAX_LINE_COUNT){
        all_line_offsets[line_counter] = line_counter == 0 ? SEEK_SET : all_line_offsets[line_counter - 1] + all_line_sizes[line_counter - 1];
        while (c != '\n' && c != '\0' && c != EOF) {
            all_line_sizes[line_counter]++;
            if (read(in, &c, 1)==0) break;
        }
	    all_line_sizes[line_counter]++;
        line_counter++;
    }
    
    printf("\nCOMPUTED OFFSETS:\n");
    for (int i = 0; i < line_counter; ++i){
        unsigned int offset = all_line_offsets[i];
        unsigned int size = all_line_sizes[i];

        lseek(in, all_line_offsets[i], SEEK_SET);
        char* line = calloc(all_line_sizes[i] + 1, sizeof(char));
        read(in, line, all_line_sizes[i] + 1);
        line[all_line_sizes[i]] = '\0';

        printf("Line #%d: offset = %u, length = %u\n", 
                i+1, offset, size);
    }
    printf("\n");

    int R;
    while(1){
        printf("> Point number of line:\n");
        if (scanf("%d", &R)!=1 || R < 0 || R > line_counter){
            printf("Wrong line number\n");
	        while ((c = getchar())!='\n' && c != EOF){}
            continue;
        } 
        if (R == 0) return 0;

        int line_R = lseek(in, all_line_offsets[R - 1], SEEK_SET);
        char* line = calloc(all_line_sizes[R - 1] + 1, sizeof(char));
        read(in, line, all_line_sizes[R - 1]);
        line[all_line_sizes[R - 1]] = '\0';
        printf("Line %d: %s\n", R, line);
    }
    close(in);
    return 0;
}
