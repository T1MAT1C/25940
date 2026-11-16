#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <stdio.h>

struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableAlternativeScreen() {
  write(STDOUT_FILENO, "\033[?1049l", 8); // alt
}

void enableAlternativeScreen() {
  write(STDOUT_FILENO, "\033[?1049h", 8); // alt
  write(STDOUT_FILENO, "\033[1;1H", 6); // start
  write(STDOUT_FILENO, "\033[2J", 4); // clear
  atexit(disableAlternativeScreen);
}

#define STR_COUNT 255
#define STR_LEN 40

void to_prev_line_end()
{
  char s[32];
  int n = snprintf(s, sizeof s, "\033[%dC", STR_LEN); // to end
  write(STDOUT_FILENO, "\033[F", 3); // prev line
  write(STDOUT_FILENO, s, (size_t)n);
}

int get_line(int len)
{
  return len / STR_LEN - (((len % STR_LEN) == 0) && len > 0);
}

int get_line_pos(int len)
{
  return ((len % STR_LEN == 0) && len > 0) ? STR_LEN : (len % STR_LEN);
}

int main()
{
  enableRawMode();
  enableAlternativeScreen();

  write(STDOUT_FILENO, "ENTER YOUR INPUT:\n", 19);

  char c;
  int words[STR_LEN*STR_COUNT] = {0};
  int word_count = 0;
  int len = 0;
  _Bool prev_ws = 0;
  while (read(STDIN_FILENO, &c, 1) == 1)
  {
    if (iscntrl(c)) 
    {
      if (c == orig_termios.c_cc[VERASE])
      { // ERASE CHAR
        if (len <= 0) continue;

        write(STDOUT_FILENO, "\033[D", 3); // left 1
        write(STDOUT_FILENO, "\033[K", 3); // erase
        len--;
        if (len % STR_LEN == 0 && len > 0)
          to_prev_line_end();

        while (word_count > 0 && words[word_count - 1] > len)
          word_count--;
      }
      else if (c == orig_termios.c_cc[VKILL])
      { // ERASE LINE
        if (len <= 0) continue;

        int newlen = len - get_line_pos(len);

        // delete words
        while (word_count > 0 && words[word_count - 1] > newlen)
          word_count--;

        write(STDOUT_FILENO, "\r\033[K", 4);
        len = newlen;
        if (len > 0) to_prev_line_end();
      }
      else if (c == orig_termios.c_cc[VEOF])
      { // EXIT
        if (len == 0) break;
      }
      else if (c == orig_termios.c_cc[VWERASE])
      { // ERASE WORD
        int newlen = 0;
        if (word_count > 0)
          newlen = words[--word_count];
        int curline = get_line(len);
        int newline = get_line(newlen);
        int lines_back = curline - newline;
        int line_pos = get_line_pos(newlen);

        // lines back
        char s[32];
        while (lines_back--)
        {
          write(STDOUT_FILENO, "\r\033[K", 4); // erase
          write(STDOUT_FILENO, "\033[F", 3); // prev line
        }
        // chars back
        write(STDOUT_FILENO, "\r", 1);
        if (line_pos > 0) 
        {
          int n = snprintf(s, sizeof s, "\033[%dC", line_pos); // to word
          write(STDOUT_FILENO, s, (size_t)n);
        }
        else if (newlen > 0)
        { // if word at beginning
          write(STDOUT_FILENO, "\r\033[K", 4); // erase
          to_prev_line_end();
        }
        write(STDOUT_FILENO, "\033[K", 3); // erase

        len = newlen;
      }
      else
      { // BEL
        write(STDOUT_FILENO, "\007", 1); // bell
      }
    } 
    else 
    {
      if (len % STR_LEN == 0 && len > 0)
        write(STDOUT_FILENO, "\n", 1);

      if (isspace(c) && !prev_ws)
        words[word_count++] = len;

      prev_ws = isspace(c);

      write(STDOUT_FILENO, &c, 1);

      len++;
    }
  }
  write(STDOUT_FILENO, "\n", 1);
}
