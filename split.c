#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char *st = "GET /index.html HTTP/1.1\r\nHost: something.com\r\nAccept: */*\r\nContent-Length: 345\r\n";
  printf("%s\n", st);
  char *ch;
  ch = strtok(st, "\r\n");
  printf("%s\n", ch);
  ch = strtok(ch, " ");
  printf("%s\n", ch);
  ch = strtok(NULL, " ");
  printf("%s\n", ch);
  return 0;
}
