#include <limits.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/unistd.h>
#include <unistd.h>
#include <stdlib.h>
#include <ulimit.h>

extern char** environ;

int main(int argc, char* argv[]){
    int opt;
    char valid_options[] = "ispuU:cC:dvV:";
    struct rlimit rl;
    char path[PATH_MAX];
    int opt_count = 0;
    struct { int opt; char *arg; } opts[100];

    printf("> Valid options: [%s]\n", valid_options);

    while ((opt = getopt(argc, argv, valid_options))!= -1){
        opts[opt_count].opt = opt;
	opts[opt_count].arg = optarg;
        opt_count++;
    }

    int arg = argc - 1;
    for (int i = opt_count - 1; i >= 0; --i){
        switch (opts[i].opt){
            case 'i':
                printf("> RUID: %d\n", getuid());
                printf("  EUID: %d\n", geteuid());
                printf("  RGID: %d\n", getgid());
                printf("  EGID: %d\n", getegid());
                break;
            case 's':
                setpgid(0, 0);
                printf("> Process %d setted as leader\n", getpid());
                break;
            case 'p':
                printf("> PID: %d\n", getpid());
                printf("  PPID: %d\n", getppid());
                printf("  PGID: %d\n", getpgid(0));
                break;
            case 'u':
		system("ulimit -u");
    		break;
            case 'U':
                getrlimit(RLIMIT_AS, &rl);
                rl.rlim_cur = atol(opts[i].arg);
                if (setrlimit(RLIMIT_AS, &rl) == -1) perror("setrlimit");
                printf("> Ulimit setted successfully\n");
                break;
            case 'c':
                if (getrlimit(RLIMIT_CORE, &rl)==0){
			if (rl.rlim_cur == RLIM_INFINITY) printf("Core-file size: unlimited\n");
                	else printf("Core-file size: %lu\n", (unsigned long)rl.rlim_cur);
		}
                break;
            case 'C':
                getrlimit(RLIMIT_CORE, &rl);
		if (atol(opts[i].arg) < 0 || (atol(opts[i].arg) == 0 && opts[i].arg[0]!='0')){
			perror("setrlimit");
			break;
		}
                rl.rlim_cur = atol(opts[i].arg);
                if (setrlimit(RLIMIT_CORE, &rl) == -1) perror("setrlimit");
                printf("> New core-file size setted successfully\n");
                break;
            case 'd':
                printf("> Path: %s\n",getcwd(path, sizeof(path)));
                break;
            case 'v':
                printf("> Environmental variables:\n");
                for (char **env = environ; *env != NULL; env++) {
                    printf("  %s\n", *env);
                }
                break;
            case 'V':
                putenv(opts[i].arg);
                printf("> New environmental variable %s setted successfully\n", opts[i].arg);
		break;
            default:
                perror("invalid option");
        }
    }
}
