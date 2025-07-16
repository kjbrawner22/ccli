#include <ccli/ccli.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_map.h"

// TODO:
//  - command groups (like Click) for subcommands
//  - global options

// POSSIBLE FEATURES:
// - prompt user for input on arguments/options

/******************** printing ********************/

static void start_color_print(CcliColor color)
{
  // print color escape sequence
  switch (color) {
    case COLOR_RED: printf("\033[0;31m"); break;
    case COLOR_GREEN: printf("\033[0;32m"); break;
    case COLOR_YELLOW: printf("\033[0;33m"); break;
    case COLOR_BLUE: printf("\033[0;34m"); break;
    case COLOR_MAGENTA: printf("\033[0;35m"); break;
    case COLOR_CYAN: printf("\033[0;36m"); break;
    default: printf("invalid color code -> %d\n", color); return;
  }
}

static void reset_color_print()
{
  printf("\033[0m");
}

static void print_color_va_args(const char* format, va_list args, CcliColor color)
{
  start_color_print(color);
  vprintf(format, args);
  reset_color_print();
}

static void _error(const char* func, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  fprintf(stderr, "[ %s ] -> Error: ", func);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  va_end(args);
  exit(1);
}

#define error(format, args...) (_error(__FUNCTION__, format, ##args))

/******************** ccli_arg ********************/

struct CcliArg {
  char* name;
  char* description;
  CcliValue value;
};

/******************** ccli_option ********************/

struct CcliOption {
  char* long_option;
  char* short_option;
  char* description;
  CcliValue value;
};

// static ccli_iterator *ccli_table_values(ccli_table *table) {
//   ccli_iterator *head = ccli_iterator_new();
//   table_entry *entry = table->entries;

//   ccli_iterator *iter = head;
//   for (int i = 0; i < table->capacity; i++) {
//     if (entry[i].key) {
//       iter = ccli_iterator_add(iter, entry[i].option);
//     }
//   }

//   if (!head->value) {
//     free(head); return NULL;
//   }

//   return head;
// }

/******************** ccli_command ********************/

struct CcliCommand {
  char* command;
  char* description;
  // ccli_command_callback callback;
  CcliHashMap* options;
  CcliArg* args;
  int argCount; // number of arguments
};

/******************** ccli - main interface ********************/

typedef struct CommandNode {
  ccli_command* command;
  struct CommandNode* next;
} CommandNode;

struct ccli {
  char* exeName;
  int argc;
  int current_arg;
  char** argv;
  char* description;
  FILE* fp;
  CommandNode* commands; // linked list of commands
  ccli_command* invoked_command;
};

ccli* ccli_init(char* exeName, int argc, char** argv)
{
  ccli* interface = malloc(sizeof(ccli));
  interface->exeName = exeName;
  interface->argc = argc;
  interface->current_arg = 1;
  interface->argv = argv;
  interface->description = NULL;
  interface->fp = stdout;

  interface->invoked_command = NULL;
  return interface;
}

void ccli_free(ccli* interface)
{
  free(interface);
}

void ccli_set_output_stream(ccli* interface, FILE* fp)
{
  interface->fp = fp;
}

void ccli_set_description(ccli* interface, char* description)
{
  interface->description = description;
}

/******************** ccli print utilities ********************/

void ccli_print(ccli* interface, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(interface->fp, format, args);
  va_end(args);
}

void ccli_print_color(ccli* interface, CcliColor color, const char* format, ...)
{
  va_list args;
  va_start(args, format);

  if (interface->fp != stdout) {
    vfprintf(interface->fp, format, args);
    va_end(args);
    return;
  }

  print_color_va_args(format, args, color);
  va_end(args);
}

// print a line to the filestream and append a newline
void ccli_echo(ccli* interface, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(interface->fp, format, args);
  va_end(args);
  fputc('\n', interface->fp);
}

/**
 * @brief
 *  print a line to the terminal in color, and append a newline.
 *  short-circuit to no color codes if the given file stream is not stdout
 *
 * @param interface
 * @param color
 * @param format
 * @param ...
 */
void ccli_echo_color(ccli* interface, CcliColor color, const char* format, ...)
{
  va_list args;
  va_start(args, format);

  if (interface->fp != stdout) {
    vfprintf(interface->fp, format, args);
    va_end(args);
    fputc('\n', interface->fp);
    return;
  }

  print_color_va_args(format, args, color);
  va_end(args);
  fputc('\n', interface->fp);
}

#define ccli_runtime_error(interface, format, args...)                                             \
  do {                                                                                             \
    ccli_print_color(interface, COLOR_RED, "Error: ");                                             \
    ccli_echo_color(interface, COLOR_RED, format, ##args);                                         \
    exit(1);                                                                                       \
  } while (false)

static void ccli_option_display(ccli* interface, ccli_option* option)
{
  // must supply a long (--double-dash) option
  ccli_print_color(interface, COLOR_YELLOW, "  %s", option->long_option);

  /*
  if (option->short_option) {
    ccli_print_color(interface, COLOR_YELLOW, ", %s", option->short_option);
  }
  */

  switch (option->type) {
    case VAL_NULL: break;
    case VAL_NUM: ccli_print_color(interface, COLOR_CYAN, "=NUMBER"); break;
    case VAL_BOOL: ccli_print_color(interface, COLOR_CYAN, "=BOOLEAN"); break;
    case VAL_STRING: ccli_print_color(interface, COLOR_CYAN, "=STRING"); break;
    default:
      ccli_option_display(interface, option);
      ccli_runtime_error(interface, "invalid value type: '%d'.", option->type);
  }

  if (option->description) {
    ccli_print_color(interface, COLOR_YELLOW, " -> %s\n", option->description);
  }

  ccli_print(interface, "\n");
}

static void ccli_display_options(ccli* interface, ccli_command* command)
{
  ccli_iterator* values = ccli_table_values(&command->options);

  if (!values) return;

  ccli_echo_color(interface, COLOR_YELLOW, "Options:");

  for (; !ccli_iterator_done(values); values = ccli_iterator_next(values)) {
    ccli_option* option = ccli_iterator_get(values, ccli_option*);
    ccli_option_display(interface, option);
  }

  ccli_print(interface, "\n");
}

static void ccli_arg_display(ccli* interface, ccli_arg* arg)
{
  ccli_print_color(interface, COLOR_YELLOW, "  %s", arg->name);

  switch (arg->type) {
    case VAL_NUM: ccli_print_color(interface, COLOR_CYAN, " (NUMBER)"); break;
    case VAL_BOOL: ccli_print_color(interface, COLOR_CYAN, " (BOOLEAN)"); break;
    case VAL_STRING: ccli_print_color(interface, COLOR_CYAN, " (STRING)"); break;
    default:
      // Unreachable
      ccli_runtime_error(interface, "unrecognized value type: '%d'.", arg->type);
  }

  if (arg->description) { ccli_print_color(interface, COLOR_YELLOW, " -> %s", arg->description); }

  ccli_print(interface, "\n");
}

static void ccli_display_args(ccli* interface, ccli_command* command)
{
  arg_array* array = &command->args;

  if (array->size <= 0) return;

  ccli_echo_color(interface, COLOR_YELLOW, "Arguments:");
  for (int i = 0; i < array->size; i++) {
    ccli_print_color(interface, COLOR_YELLOW, "  %d.", i);
    ccli_arg_display(interface, array->args[0]);
  }

  ccli_print(interface, "\n");
}

static void ccli_detailed_command_display(ccli* interface, ccli_command* command)
{
  ccli_print_color(
    interface, COLOR_YELLOW, "Usage: ./%s %s [OPTIONS]", interface->exeName, command->command
  );

  for (int i = 0; i < command->args.size; i++) {
    ccli_print_color(interface, COLOR_YELLOW, " <%s>", command->args.args[i]->name);
  }

  ccli_print(interface, "\n\n");

  if (command->description) {
    ccli_echo_color(interface, COLOR_YELLOW, "  %s\n", command->description);
  }

  ccli_display_options(interface, command);
  ccli_display_args(interface, command);
}

static void ccli_command_display(ccli* interface, ccli_command* command)
{
  ccli_print_color(interface, COLOR_YELLOW, "%s", command->command);
  if (command->description) {
    ccli_print_color(interface, COLOR_YELLOW, " -> %s", command->description);
  }
  ccli_print(interface, "\n");
}

static void ccli_display_commands(ccli* interface)
{
  ccli_echo_color(interface, COLOR_YELLOW, "Commands:");
  for (int i = 0; i < interface->commands.size; i++) {
    ccli_print(interface, "  ");
    ccli_command_display(interface, interface->commands.commands[i]);
  }
}

static void ccli_usage(ccli* interface)
{
  ccli_echo_color(interface, COLOR_YELLOW, "Usage: ./%s [command] [options]\n", interface->exeName);
}

static void ccli_display(ccli* interface)
{
  ccli_usage(interface);

  if (interface->description) {
    ccli_echo_color(interface, COLOR_YELLOW, "  %s\n", interface->description);
  }

  // TODO: commands help
  ccli_display_commands(interface);
  ccli_print(interface, "\n");

  // TODO: global options help
}

/******************** ccli global interface API ********************/

ccli_command* ccli_add_command(ccli* interface, char* command, ccli_command_callback callback)
{
  ccli_command* _command = ccli_command_new(command, callback);
  ccli_command_add_option(_command, "--help", NULL, VAL_NULL);

  command_array_add(&interface->commands, _command);
  return _command;
}

void ccli_help(ccli* interface, ccli_command* command)
{
  if (!command) {
    // global help
    ccli_display(interface);
    return;
  }

  ccli_command_display(interface, command);
}

static ccli_command* get_command(ccli* interface)
{
  for (int i = 0; i < interface->commands.size; i++) {
    if (!strcmp(interface->argv[1], interface->commands.commands[i]->command)) {
      interface->current_arg++;
      return interface->commands.commands[i];
    }
  }

  return NULL;
}

typedef struct {
  char* name;
  char* val;
} parsed_option;

// these helpers are mainly for readability
#define parsed_option_new(arg, val) ((parsed_option){arg, val})

static void parsed_option_free(parsed_option* option)
{
  free(option->name);
}

static char* copy_chars(char* chars, int length)
{
  char* string = malloc(sizeof(char) * (length + 1));
  strncpy(string, chars, length);
  string[length] = '\0';
  return string;
}

bool is_digit(char c)
{
  return (c >= '0' && c <= '9');
}

bool is_number(char* value)
{
  if (is_digit(value[0])) {
    return true;
  } else if (value[0] == '.') {
    return (strlen(value) > 1 && is_digit(value[1]));
  } else if (value[0] == '-') {
    return (strlen(value) > 1 && is_digit(value[1]))
           || (strlen(value) > 2 && value[1] == '.' && is_digit(value[2]));
  } else
    return false;
}

bool is_bool(char* value)
{
  return (
    !strcasecmp(value, "t") || !strcasecmp(value, "f") || !strcasecmp(value, "true")
    || !strcasecmp(value, "false")
  );
}

// returns the boolean represented by [value].
// returns false if the value isn't valid.
bool strtobool(char* value)
{
  return (!strcasecmp(value, "true") || !strcasecmp(value, "t"));
}

void set_option_value(
  ccli* interface,
  ccli_command* command,
  ccli_option* option,
  char* name,
  char* value
)
{
  if (!value) {
    if (option->type == VAL_NULL) {
      option->value = BOOL_VAL(true);
      return;
    } else {
      ccli_detailed_command_display(interface, command);
      ccli_runtime_error(interface, "missing option parameter: '%s'.", name);
    }
  }

  switch (option->type) {
    case VAL_NULL: {
      ccli_detailed_command_display(interface, command);
      ccli_runtime_error(interface, "option doesn't take parameter: '%s=%s'.", name, value);
    }
    case VAL_BOOL: {
      if (is_bool(value)) {
        option->value = BOOL_VAL(strtobool(value));
      } else {
        ccli_detailed_command_display(interface, command);
        ccli_runtime_error(interface, "invalid boolean: '%s'.", value);
      }
      break;
    }
    case VAL_NUM: {
      if (is_number(value)) {
        option->value = NUM_VAL(strtod(value, NULL));
      } else {
        ccli_detailed_command_display(interface, command);
        ccli_runtime_error(interface, "invalid number: '%s'.", value);
      }
      break;
    }
    case VAL_STRING: {
      option->value = STRING_VAL(value);
      break;
    }
    default:
      ccli_runtime_error(interface, "unrecognized value type: %d\n", option->type);
      // TODO: handle this more gracefully?
  }
}

void ccli_run(ccli* interface)
{
  if (interface->argc <= 1 || !strcmp(interface->argv[interface->current_arg], "--help")) {
    ccli_help(interface, NULL);
    return;
  }

  ccli_help(interface, NULL);
}