#ifndef ccli_h
#define ccli_h

//#define CCLI_TESTS

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

ccli *newCCLI(int argc, char **argv);
void freeCCLI(ccli *interface);
void runCCLI(ccli *interface);
void setDescriptionCCLI(ccli *interface, char *description);

#endif