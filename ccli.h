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

typedef struct ccli         ccli;
typedef struct ccli_table   ccli_table;
typedef struct ccli_command ccli_command;
typedef struct ccli_arg     ccli_arg;
typedef struct ccli_option  ccli_option;

typedef void (*ccli_command_callback)(ccli *interface, ccli_table *options);

ccli *ccli_init(char *exeName, int argc, char **argv);
void ccli_free(ccli *interface);
void ccli_run(ccli *interface);
void ccli_set_description(ccli *interface, char *description);
void ccli_set_output_stream(ccli *interface, FILE *fp);

// functions for retrieving option values.
//
// returns true if the option was specified in the command,
// and is of the appropriate type
bool ccli_table_exists(ccli_table *options, char *option);
bool ccli_table_get_int(ccli_table *options, char *option, int *value);
bool ccli_table_get_double(ccli_table *options, char *option, double *value);
bool ccli_table_get_bool(ccli_table *options, char *option, bool *value);
bool ccli_table_get_string(ccli_table *options, char *name, char **value);

// functions for retrieving argument values.
//
// fails if you try to get an inappropriate type from an argument.
int ccli_get_int_arg(ccli *interface, int index);
double ccli_get_double_arg(ccli *interface, int index);
bool ccli_get_bool_arg(ccli *interface, int index);
char *ccli_get_string_arg(ccli *interface, int index);

ccli_command *ccli_add_command(ccli *interface, char *command, ccli_command_callback callback);
void ccli_command_set_description(ccli_command *command, char *description);

ccli_arg *ccli_command_add_number_arg(ccli_command *command, char *name);
ccli_arg *ccli_command_add_bool_arg(ccli_command *command, char *name);
ccli_arg *ccli_command_add_string_arg(ccli_command *command, char *name);
void ccli_arg_set_description(ccli_arg *arg, char *description);
//TODO: replace with functions for each value type, rather than exposing the enumeration.
//      reduces possibility for errors and makes the interface a little cleaner.
//      downside is more distinct function calls.
ccli_option *ccli_command_add_option(ccli_command *command, char *double_dash_option,
                               char *single_dash_option, ccli_value_type type);
void ccli_option_set_description(ccli_option *option, char *description);
void ccli_option_set_default_number(ccli_option *option, double value);
void ccli_option_set_default_bool(ccli_option *option, bool value);
void ccli_option_set_default_string(ccli_option *option, char *value);

void ccli_echo(ccli *interface, const char *format, ...);
void ccli_echo_color(ccli *interface, ccli_color color, const char *format, ...);

#endif