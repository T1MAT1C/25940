#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void print_ids(){
  printf("REAL UID: %d\n", (int)getuid());
  printf("EFFECTIVE UID: %d\n", (int)geteuid());
}

void check_file(){
  FILE *file = fopen("file", "r+");
  if (file == NULL){
    perror("fopen");
    return;
  }
  printf("File has opened successfully\n");
  fclose(file);
}

int main(){
  printf("\nBefore setuid:\n");
  print_ids();
  check_file();
  if (setuid(getuid()) == -1){
    perror("setuid");
    return EXIT_FAILURE;
  }

  printf("\nAfter setuid:\n");
  print_ids();
  check_file();
  return EXIT_SUCCESS;
}
