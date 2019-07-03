#ifndef ccli_h
#define ccli_h

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

typedef enum {
  VAL_NULL,
  VAL_NUM,
  VAL_BOOL,
  VAL_STRING
} ccli_value_type;

typedef struct ccli ccli;
typedef struct ccli_value ccli_value;
typedef struct ccli_table ccli_table;
typedef struct ccli_command ccli_command;
typedef struct ccli_option ccli_option;

typedef void (*ccli_command_callback)(ccli *interface);

ccli_value *ccli_int(int value);
ccli_value *ccli_bool(bool value);
ccli_value *ccli_string(char *string);

ccli *ccli_init(char *exeName, int argc, char **argv);
void ccli_set_description(ccli *interface, char *description);
void ccli_set_output_stream(ccli *interface, FILE *fp);
ccli_command *ccli_add_command(ccli *interface, char *command, ccli_command_callback callback);
ccli_option *ccli_command_add_option(ccli_command *command, char *double_dash_option, 
                               char *single_dash_option, ccli_value_type type);
void ccli_command_set_description(ccli_command *command, char *description);
void ccli_echo(ccli *interface, const char *format, ...);
void ccli_echo_color(ccli *interface, ccli_color color, const char *format, ...);
void ccli_run(ccli *interface);
void ccli_free(ccli *interface);

#endif