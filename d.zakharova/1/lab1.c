#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <ulimit.h>
#include <stdlib.h>
#include <stdio.h>
#define GET_FSLIM 1
#define SET_FSLIM 2
extern char **environ; // переменные среды

main(int argc, char *argv[])
{
    int c; 
    char options[ ] = "ispuU:cC:dvV:";
    struct rlimit rlp;
    char **p;

    if(argc < 2) {
        fprintf(stderr,"Usage: %s options\n", argv[0]);
        exit(0);
    }

    while ((c = getopt(argc, argv, options)) != EOF)
        switch (c) {
        case 'i':
            printf("real userid = %ld\n", getuid());
            printf("effective userid = %ld\n", geteuid());
            printf("real groupid = %ld\n", getgid());
            printf("effective groupid = %ld\n", getegid());
            break;
        case 's':
            (void) setpgid(0,0);
            break;
        case 'p':
            printf("process number = %ld\n", getpid());
            printf("parent process number = %ld\n", getppid());
            printf("group process number = %ld\n", getpgid(0));
            break;
        case 'U':
            if( ulimit(SET_FSLIM, atol(optarg) ) == -1)
                fprintf(stderr,"Must be super-user to increase ulimit\n");
            break;
        case 'u':
            printf("ulimit = %ld\n", ulimit(GET_FSLIM, 0) );
            break;
        case 'c':
            getrlimit(RLIMIT_CORE, &rlp);
            printf("core size = %ld\n", rlp.rlim_cur);
            break;
        case 'C':
            getrlimit(RLIMIT_CORE, &rlp);
            rlp.rlim_cur = atol(optarg);
            if (setrlimit(RLIMIT_CORE, &rlp) == -1)
                fprintf(stderr, "Must be super-user to increase core\n");
            break;
        case 'd':
            printf("current working directory is: %s\n",
                   getcwd(NULL,100));
            break;
        case 'v':
            printf("environment variables are:\n");
            for (p = environ; *p; p++)
                printf("%s\n", *p);
            break;
        case 'V':
            putenv(optarg);
            break;
        }
}
