#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char st[] = "GET /index.html HTTP/1.1\r\nHost: something.com\r\nAccept: */*\r\nContent-Length: 345\r\n";
  char *ch;
  ch = strtok(st, "\r\n");
  ch = strtok(ch, " ");
  ch = strtok(NULL, " ");
  return 0;
}
