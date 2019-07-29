#ifndef ccli_h
#define ccli_h

#define CCLI_TESTS 1

#include <stdio.h>
#include <stdbool.h>

typedef enum {
  COLOR_RED,
  COLOR_GREEN,
  COLOR_YELLOW,
  COLOR_BLUE,
  COLOR_MAGENTA,
  COLOR_CYAN
} ccli_color;

typedef struct ccli ccli;

typedef void (*ccli_command_callback)(ccli *interface);

ccli *ccliNew(int argc, char **argv);
void ccliFree(ccli *interface);
void ccliRun(ccli *interface);
void ccliSetDescription(ccli *interface, char *description);
void ccliSetOutputStream(ccli *interface, FILE *fp);

#endif