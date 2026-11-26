#include <sys/types.h>
#include <osfcn.h>
#include <ctype.h>

int main(int argc, char** argv)
{
	int pid;
	int fd[2];
	char* text = { "qwertyuiop\nasdfghjkl\n zxcvbnm\n" };
	char input[1000];
	int i;
	int rtn;
	if (pipe(fd) == -1) {
		error("cannot open a pipe");
		return 1;
	}
	if ((pid = fork()) > 0) {
		close(fd[0]);
			write(fd[1], text, strlen(text));
		close(fd[1]);
	}
	else if (pid == 0) {
		close(fd[1]);
		while ((rtn = read(fd[0], input, 1000)) > 0) {
			for (i = 0; i < rtn; i++)
				if (islower(input[i]))
				input[i] = toupper(input[i]);
			write(1, input, rtn);
		}
		close(fd[0]);
	}
	else {
		perror("fork failure");
		return 2;
	}

	return 0;
}