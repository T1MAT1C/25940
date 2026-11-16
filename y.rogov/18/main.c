#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>

int main(int argc, const char *argv[])
{
  for (int a = 1; a < argc; ++a)
  {
    // stat
    const char *path = argv[a];
    struct stat st;
    if (stat(path, &st) == -1)
    {
      perror("stat");
      return EXIT_FAILURE;
    }

    // dir
    char dir = S_ISDIR(st.st_mode) ? 'd' 
             : S_ISREG(st.st_mode) ? '-' : '?';
    // user perm
    char rusr = S_IRUSR & st.st_mode ? 'r' : '-';
    char wusr = S_IWUSR & st.st_mode ? 'w' : '-';
    char xusr = S_IXUSR & st.st_mode ? 'x' : '-';
    // group perm
    char rgrp = S_IRGRP & st.st_mode ? 'r' : '-';
    char wgrp = S_IWGRP & st.st_mode ? 'w' : '-';
    char xgrp = S_IXGRP & st.st_mode ? 'x' : '-';
    // other perm
    char roth = S_IROTH & st.st_mode ? 'r' : '-';
    char woth = S_IWOTH & st.st_mode ? 'w' : '-';
    char xoth = S_IXOTH & st.st_mode ? 'x' : '-';

    // user
    struct passwd *pw = getpwuid(st.st_uid);
    char * pw_name = pw->pw_name;
    // group
    struct group *gr = getgrgid(st.st_gid);
    char * gr_name = gr->gr_name;

    // filesize
    char size[64] = "\0";
    if (dir == '-')
      snprintf(size, sizeof(size), "%ld", st.st_size);

    // time
    char time_str[64];
    time_t t = st.st_mtim.tv_sec;
    struct tm *tm = localtime(&t);
    strftime(time_str, sizeof(time_str), "%b %e %H:%M", tm);

    // name
    const char *slash = strrchr(argv[a], '/');
    const char *name = slash ? slash + 1 : argv[a];

    printf(
      "%c%c%c%c%c%c%c%c%c%c %8s %8s %8s %14s %-15s\n", 
      dir,
      rusr, wusr, xusr,
      rgrp, wgrp, xgrp,
      roth, woth, xoth,
      pw_name, gr_name, 
      size,
      time_str,
      name
    );
  }
}
