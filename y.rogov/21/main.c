#include <unistd.h>
#include <signal.h>

sig_atomic_t count = 0;

int itoa(int n, char buf[])
{
  int len = 0;

  if (n == 0) buf[len++] = '0';
  
  while (n)
  {
    buf[len++] = '0' + n % 10;
    n /= 10;
  }

  for (int i = 0; i < len / 2; ++i)
  {
    char t = buf[i];
    buf[i] = buf[len - i - 1];
    buf[len - i - 1] = t;
  }

  return len;
}

void sig_handler(int sig)
{
  write(STDOUT_FILENO, "\033[G", 3);
  write(STDOUT_FILENO, "\033[K", 3);

  if (sig == SIGINT)
  {
    write(STDOUT_FILENO, "\a", 2);
    count++;
  }
  else if (sig == SIGQUIT)
  {
    char buf[64] = {0};
    int len = itoa(count, buf);
    write(STDOUT_FILENO, "BELL USED ", 10);
    write(STDOUT_FILENO, buf, len);
    write(STDOUT_FILENO, " TIMES. EXITING...\n", 19);
    _exit(0);
  }
}

int main()
{
  struct sigaction sa;
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  while(1);
}
