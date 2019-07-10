#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ccli.h"

//TODO:
// - command groups (like Click) for subcommands
// - global options


// POSSIBLE FEATURES:
// - prompt user for input on arguments/options

#define GROW_ARRAY_CAPACITY(cap) ((cap == 0) ? 8 : (cap) * 2)

/******************** ccli_iterator ********************/

typedef struct ccli_iterator {
  void *value;
  struct ccli_iterator *next;
} ccli_iterator;

ccli_iterator *ccli_iterator_new() {
  ccli_iterator *iterator = malloc(sizeof(ccli_iterator));
  iterator->value = NULL;
  iterator->next = NULL;
  return iterator;
}

// NOTE: using a macro here to make type-cast more readable.
// if we expose the opaque iterator object publicly, this won't
// work.
#define ccli_iterator_get(iter, type) ((type)iter->value)

// the iterator passed to this function should most likely be NULL.
// if not, its value will be replaced with the given value.
//
// Remember to maintain a pointer to the head of the iterator
// before using this function.
static ccli_iterator *ccli_iterator_add(ccli_iterator *iterator, void *value) {
  if (!iterator) return NULL;

  iterator->value = value;
  iterator->next = ccli_iterator_new();
  return iterator->next;
}

// return true if there's another iteration
static ccli_iterator *ccli_iterator_next(ccli_iterator *iterator) {
  if (!iterator) return NULL;

  return iterator->next;
}

static bool ccli_iterator_done(ccli_iterator *iterator) {
  if (!iterator || !iterator->value)  {
    free(iterator);
    return true;
  }

  return false;
}

/******************** ccli_value ********************/

typedef struct {
  ccli_value_type type;
  union {
    double number;
    bool boolean;
    char *string;
  } as;
} ccli_value;

#define NULL_VAL          ((ccli_value){ VAL_NULL,   { .number = 0 } })
#define BOOL_VAL(value)   ((ccli_value){ VAL_BOOL,   { .boolean = value } })
#define NUM_VAL(value)    ((ccli_value){ VAL_NUM,    { .number = (double)value } })
#define STRING_VAL(value) ((ccli_value){ VAL_STRING, { .string = value }})

#define IS_NULL(value)    ((value).type == VAL_NULL)
#define IS_NUM(value)     ((value).type == VAL_NUM)
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_STRING(value)  ((value).type == VAL_STRING)

#define AS_INT(value)     ((int)((value).as.number))
#define AS_DOUBLE(value)  ((value).as.number)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_STRING(value)  ((value).as.string)

/******************** ccli_arg ********************/

struct ccli_arg {
  char *name;
  char *description;
  ccli_value_type type;
  ccli_value value;
};

static ccli_arg *ccli_arg_new(char *name, ccli_value_type type) {
  ccli_arg *arg = malloc(sizeof(ccli_arg));
  arg->name = name;
  arg->description = NULL;
  arg->type = type;
  arg->value = NULL_VAL;
  return arg;
}

// might need this helper later, rather than just a wrapper
// around a free call
static void ccli_arg_free(ccli_arg *arg) {
  free(arg);
}

void ccli_arg_set_description(ccli_arg *arg, char *description) {
    arg->description = description;
}

/******************** arg_array ********************/

typedef struct {
  int size;
  int capacity;
  ccli_arg **args;
} arg_array;

static void arg_array_init(arg_array *array) {
  array->size = 0;
  array->capacity = 0;
  array->args = NULL;
}

static void arg_array_free(arg_array *array) {
  for (int i = 0; i < array->size; i++) {
    ccli_arg_free(array->args[i]);
  }

  free(array->args);

  arg_array_init(array);
}

static void arg_array_add(arg_array *array, ccli_arg *arg) {
  if (array->size + 1 > array->capacity) {
    array->capacity = GROW_ARRAY_CAPACITY(array->capacity);
    array->args = realloc(array->args, sizeof(ccli_arg *) * array->capacity);
  }

  array->args[array->size++] = arg;
}

/******************** ccli_option ********************/

struct ccli_option {
  char *long_option;
  char *short_option;
  char *description;
  ccli_value_type type;
  ccli_value value;
};

static ccli_option *ccli_option_new(char *double_dash_option, char *single_dash_option, ccli_value_type type) {
  ccli_option *option = malloc(sizeof(ccli_option));
  option->long_option = double_dash_option;
  option->short_option = single_dash_option;
  option->description = NULL;
  option->type = type;
  option->value = NULL_VAL;
  return option;
}

void ccli_option_set_description(ccli_option *option, char *description) {
  option->description = description;
}

void ccli_option_set_default_number(ccli_option *option, double value) {
  if (option->type != VAL_NUM) {
    // error
  }

  option->value = NUM_VAL(value);
}

void ccli_option_set_default_bool(ccli_option *option, bool value) {
  if (option->type != VAL_BOOL) {
    // error
  }

  option->value = BOOL_VAL(value);
}

void ccli_option_set_default_string(ccli_option *option, char *value) {
  if (option->type != VAL_STRING) {
    // error
  }

  option->value = STRING_VAL(value);
}

/******************** ccli_table ********************/

#define TABLE_MAX_LOAD 0.75

typedef struct {
  char *chars;
  uint32_t hash;
} table_string;

uint32_t hash_string(const char *key) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < strlen(key); i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

table_string *table_string_new(char *chars) {
  table_string *string = malloc(sizeof(table_string));
  string->chars = chars;
  string->hash = hash_string(chars);
  return string;
}

typedef struct {
  table_string *key;
  ccli_option *option;
} table_entry;

typedef struct {
  table_entry *entries;
  int count;
  int capacity;
} ccli_table;

void ccli_table_init(ccli_table *table) {
  table->capacity = 0;
  table->count = 0;
  table->entries = NULL;
}

void ccli_table_free(ccli_table *table) {
  for (int i = 0; i < table->capacity; i++) {
    table_string *string = table->entries[i].key;
    ccli_option *option = table->entries[i].option;
    if (string) free(string);
    if (option) free(option);
  }

  free(table->entries);
  ccli_table_init(table);
}

// find an entry or its respective spot in the table
static table_entry *ccli_table_find_entry(table_entry *entries, int capacity, table_string *key) {
  if (!entries) return NULL;

  uint32_t index = key->hash % capacity;
  table_entry *tombstone = NULL;

  for (;;) {
    table_entry *entry = &entries[index];

    if (!entry->key) {
      if (!entry->option) {
        // empty entry, return tombstone entry if found
        return (tombstone != NULL) ? tombstone : entry;
      } else {
        // found a tombstone
        if (!tombstone) tombstone = entry;
      }
    } else if (entry->key == key) {
      // found the key
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void ccli_table_adjust_capacity(ccli_table *table, int capacity) {
  table_entry *entries = malloc(sizeof(table_entry) * capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].option = NULL;
  }

  // don't copy over tombstones, reset and reconstruct the table
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    table_entry *entry = &table->entries[i];

    // disregard tombstones and empty slots
    if (!entry->key) continue;

    table_entry *dest = ccli_table_find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->option = entry->option;
    table->count++;
  }

  free(table->entries);

  table->entries = entries;
  table->capacity = capacity;
}

bool ccli_table_get(ccli_table *table, table_string *key, ccli_option **option) {
  if (!table->entries) return false;

  table_entry *entry = ccli_table_find_entry(table->entries, table->capacity, key);
  if (!entry->key) return false;

  *option = entry->option;
  return true;
}

static bool ccli_table_set(ccli_table *table, table_string *key, ccli_option *option) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_ARRAY_CAPACITY(table->capacity);
    ccli_table_adjust_capacity(table, capacity);
  }

  table_entry *entry = ccli_table_find_entry(table->entries, table->capacity, key);

  bool isNewKey = (entry->key == NULL);
  // increment count if it isn't a real value or a tombstone
  if (isNewKey && !entry->option) table->count++;

  entry->key = key;
  entry->option = option;
  return isNewKey;
}

static ccli_iterator *ccli_table_values(ccli_table *table) {
  ccli_iterator *head = ccli_iterator_new();
  table_entry *entry = table->entries;

  ccli_iterator *iter = head;
  for (int i = 0; i < table->capacity; i++) {
    if (entry[i].key) {
      iter = ccli_iterator_add(iter, entry[i].option);
    }
  }

  if (!head->value) {
    free(head); return NULL;
  }

  return head;
}

static table_string *ccli_table_find_string(ccli_table *table, const char *chars) {
  if (!table->entries) return NULL;

  uint32_t hash = hash_string(chars);

  uint32_t index = hash % table->capacity;

  for (;;) {
    table_entry *entry = &table->entries[index];
    if (!entry->key) {
      // stop if we find an empty, non-tombstone entry
      if (!entry->option) return NULL;
    } else if (!strcmp(entry->key->chars, chars)) {
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

bool ccli_table_exists(ccli_table *table, char *name) {
  ccli_option *option = NULL;
  table_string *string = ccli_table_find_string(table, name);
  if (string && ccli_table_get(table, string, &option)) {
    if (!IS_NULL(option->value)) return true;
  }

  return false;
}

bool ccli_table_get_int(ccli_table *table, char *name, int *value) {
  ccli_option *option = NULL;
  table_string *string = ccli_table_find_string(table, name);
  if (string && ccli_table_get(table, string, &option)) {
    if (IS_NUM(option->value)) {
      *value = AS_INT(option->value);
      return true;
    }
  }

  return false;
}

bool ccli_table_get_double(ccli_table *table, char *name, double *value) {
  ccli_option *option = NULL;
  table_string *string = ccli_table_find_string(table, name);
  if (string && ccli_table_get(table, string, &option)) {
    if (IS_NUM(option->value)) {
      *value = AS_DOUBLE(option->value);
      return true;
    }
  }

  return false;
}

bool ccli_table_get_bool(ccli_table *table, char *name, bool *value) {
  ccli_option *option = NULL;
  table_string *string = ccli_table_find_string(table, name);
  if (string && ccli_table_get(table, string, &option)) {
    if (IS_BOOL(option->value)) {
      *value = AS_BOOL(option->value);
      return true;
    }
  }

  return false;
}

bool ccli_table_get_string(ccli_table *table, char *name, char **value) {
  ccli_option *option = NULL;
  table_string *string = ccli_table_find_string(table, name);
  if (string && ccli_table_get(table, string, &option)) {
    if (IS_STRING(option->value)) {
      *value = AS_STRING(option->value);
      return true;
    }
  }

  return false;
}

/******************** ccli_command ********************/

struct ccli_command {
  char *command;
  char *description;
  ccli_command_callback callback;
  ccli_table options;
  arg_array args;
};

static ccli_command *ccli_command_new(char *command, ccli_command_callback callback) {
  ccli_command *_command = malloc(sizeof(ccli_command));
  _command->command = command;
  _command->description = NULL;
  _command->callback = callback;
  ccli_table_init(&_command->options);
  arg_array_init(&_command->args);
  return _command;
}

static void ccli_command_free(ccli_command *command) {
  ccli_table_free(&command->options);
  arg_array_free(&command->args);
  free(command);
}

void ccli_command_set_description(ccli_command *command, char *description) {
  command->description = description;
}

ccli_arg *ccli_command_add_number_arg(ccli_command *command, char *name) {
  ccli_arg *arg = ccli_arg_new(name, VAL_NUM);
  arg_array_add(&command->args, arg);
  return arg;
}

ccli_arg *ccli_command_add_bool_arg(ccli_command *command, char *name) {
  ccli_arg *arg = ccli_arg_new(name, VAL_BOOL);
  arg_array_add(&command->args, arg);
  return arg;
}

ccli_arg *ccli_command_add_string_arg(ccli_command *command, char *name) {
  ccli_arg *arg = ccli_arg_new(name, VAL_STRING);
  arg_array_add(&command->args, arg);
  return arg;
}

ccli_option *ccli_command_add_option(ccli_command *command, char *double_dash_option,
                             char *single_dash_option, ccli_value_type type) {
  // TODO: implement global options
  // TODO: allow single dash options on their own, too
  if (!command || !double_dash_option) return NULL;

  ccli_option *option = ccli_option_new(double_dash_option, single_dash_option, type);

  table_string *string = ccli_table_find_string(&command->options, double_dash_option);
  ccli_table_set(&command->options, (string) ? string : table_string_new(double_dash_option), option);

  if (single_dash_option) {
    string = ccli_table_find_string(&command->options, single_dash_option);
    ccli_table_set(&command->options, (string) ? string : table_string_new(single_dash_option), option);
  }

  return option;
}

/********** command_array **********/

typedef struct {
  int size;
  int capacity;
  ccli_command **commands;
} command_array;

static void command_array_init(command_array *array) {
  array->size = 0;
  array->capacity = 0;
  array->commands = NULL;
}

static void command_array_free(command_array *array) {
  for (int i = 0; i < array->size; i++) {
    ccli_command_free(array->commands[i]);
  }

  free(array->commands);

  command_array_init(array);
}

static void command_array_add(command_array *array, ccli_command *command) {
  if (array->size + 1 > array->capacity) {
    array->capacity = GROW_ARRAY_CAPACITY(array->capacity);
    array->commands = realloc(array->commands, sizeof(ccli_command *) * array->capacity);
  }

  array->commands[array->size++] = command;
}

/******************** ccli - main interface ********************/

struct ccli {
  char *exeName;
  int argc;
  int current_arg;
  char **argv;
  char *description;
  FILE *fp;
  command_array commands;
  ccli_command *invoked_command;
};

ccli *ccli_init(char *exeName, int argc, char **argv) {
  ccli *interface = malloc(sizeof(ccli));
  interface->exeName = exeName;
  interface->argc = argc;
  interface->current_arg = 1;
  interface->argv = argv;
  interface->description = NULL;
  interface->fp = stdout;

  interface->invoked_command = NULL;
  command_array_init(&interface->commands);
  return interface;
}

void ccli_free(ccli *interface) {
  command_array_free(&interface->commands);
  free(interface);
}

void ccli_set_output_stream(ccli *interface, FILE *fp) {
  interface->fp = fp;
}

void ccli_set_description(ccli *interface, char *description) {
  interface->description = description;
}

/******************** ccli option retrieval ********************/

bool ccli_option_exists(ccli *interface, char *option) {
  if (!interface->invoked_command) return false;

  return ccli_table_exists(&interface->invoked_command->options, option);
}

bool ccli_get_int_option(ccli *interface, char *option, int *value) {
  if (!interface->invoked_command) return false;

  return ccli_table_get_int(&interface->invoked_command->options, option, value);
}

bool ccli_get_double_option(ccli *interface, char *option, double *value) {
  if (!interface->invoked_command) return false;

  return ccli_table_get_double(&interface->invoked_command->options, option, value);
}

bool ccli_get_bool_option(ccli *interface, char *option, bool *value) {
  if (!interface->invoked_command) return false;

  return ccli_table_get_bool(&interface->invoked_command->options, option, value);
}

bool ccli_get_string_option(ccli *interface, char *option, char **value) {
  if (!interface->invoked_command) return false;

  return ccli_table_get_string(&interface->invoked_command->options, option, value);
}

/******************** ccli print utilities ********************/

void ccli_print(ccli *interface, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(interface->fp, format, args);
  va_end(args);
}

void ccli_print_color(ccli *interface, ccli_color color, const char *format, ...) {
  va_list args;
  va_start(args, format);

  if (interface->fp != stdout) {
    vfprintf(interface->fp, format, args);
    va_end(args);
    return;
  }

  // print color escape sequence
  switch (color) {
    case COLOR_RED:     fprintf(interface->fp, "\033[0;31m"); break;
    case COLOR_GREEN:   fprintf(interface->fp, "\033[0;32m"); break;
    case COLOR_YELLOW:  fprintf(interface->fp, "\033[0;33m"); break;
    case COLOR_BLUE:    fprintf(interface->fp, "\033[0;34m"); break;
    case COLOR_MAGENTA: fprintf(interface->fp, "\033[0;35m"); break;
    case COLOR_CYAN:    fprintf(interface->fp, "\033[0;36m"); break;
    default:
      fprintf(interface->fp, "invalid color code -> %d", color); return;
  }

  vfprintf(interface->fp, format, args);
  va_end(args);

  // reset to default color
  fprintf(interface->fp, "\033[0m");
}

// print a line to the filestream and append a newline
void ccli_echo(ccli *interface, const char *format, ...) {
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
void ccli_echo_color(ccli *interface, ccli_color color, const char *format, ...) {
  va_list args;
  va_start(args, format);


  if (interface->fp != stdout) {
    vfprintf(interface->fp, format, args);
    va_end(args);
    fputc('\n', interface->fp);
    return;
  }

  // print color escape sequence
  switch (color) {
    case COLOR_RED:     fprintf(interface->fp, "\033[0;31m"); break;
    case COLOR_GREEN:   fprintf(interface->fp, "\033[0;32m"); break;
    case COLOR_YELLOW:  fprintf(interface->fp, "\033[0;33m"); break;
    case COLOR_BLUE:    fprintf(interface->fp, "\033[0;34m"); break;
    case COLOR_MAGENTA: fprintf(interface->fp, "\033[0;35m"); break;
    case COLOR_CYAN:    fprintf(interface->fp, "\033[0;36m"); break;
    default:
      fprintf(interface->fp, "invalid color code -> %d\n", color); return;
  }

  vfprintf(interface->fp, format, args);
  va_end(args);

  // reset to default color
  fprintf(interface->fp, "\033[0m");
  fputc('\n', interface->fp);
}

#define ccli_error(interface, format, args...)           \
  do {                                                   \
    ccli_print_color(interface, COLOR_RED, "Error: ");   \
    ccli_echo_color(interface, COLOR_RED, format, ## args); \
    exit(1);                                             \
  } while (false)

static void ccli_option_display(ccli *interface, ccli_option *option) {
  // must supply a long (--double-dash) option
  ccli_print_color(interface, COLOR_YELLOW, "  %s", option->long_option);

  /*
  if (option->short_option) {
    ccli_print_color(interface, COLOR_YELLOW, ", %s", option->short_option);
  }
  */

  switch (option->type) {
    case VAL_NULL:   break;
    case VAL_NUM:    ccli_print_color(interface, COLOR_CYAN, "=NUMBER"); break;
    case VAL_BOOL:   ccli_print_color(interface, COLOR_CYAN, "=BOOLEAN"); break;
    case VAL_STRING: ccli_print_color(interface, COLOR_CYAN, "=STRING"); break;
    default:
      ccli_option_display(interface, option);
      ccli_error(interface, "invalid value type: '%d'.", option->type);
  }

  if (option->description) {
    ccli_print_color(interface, COLOR_YELLOW, " -> %s\n", option->description);
  }

  ccli_print(interface, "\n");
}

static void ccli_display_options(ccli *interface, ccli_command *command) {
  ccli_iterator *values = ccli_table_values(&command->options);

  if (!values) return;

  ccli_echo_color(interface, COLOR_YELLOW, "Options:");

  for (; !ccli_iterator_done(values); values = ccli_iterator_next(values)) {
    ccli_option *option = ccli_iterator_get(values, ccli_option *);
    ccli_option_display(interface, option);
  }

  ccli_print(interface, "\n");
}

static void ccli_arg_display(ccli *interface, ccli_arg *arg) {
  ccli_print_color(interface, COLOR_YELLOW, "  %s", arg->name);

  switch(arg->type) {
    case VAL_NUM: ccli_print_color(interface, COLOR_CYAN, " (NUMBER)"); break;
    case VAL_BOOL: ccli_print_color(interface, COLOR_CYAN, " (BOOLEAN)"); break;
    case VAL_STRING: ccli_print_color(interface, COLOR_CYAN, " (STRING)"); break;
    default:
      // Unreachable
      ccli_error(interface, "unrecognized value type: '%d'.", arg->type);

  }

  if (arg->description) {
    ccli_print_color(interface, COLOR_YELLOW, " -> %s", arg->description);
  }

  ccli_print(interface, "\n");
}

static void ccli_display_args(ccli *interface, ccli_command *command) {
  arg_array *array = &command->args;

  if (array->size <= 0) return;

  ccli_echo_color(interface, COLOR_YELLOW, "Arguments:");
  for (int i = 0; i < array->size; i++) {
    ccli_print_color(interface, COLOR_YELLOW, "  %d.", i);
    ccli_arg_display(interface, array->args[0]);
  }

  ccli_print(interface, "\n");
}

static void ccli_detailed_command_display(ccli *interface, ccli_command *command) {
  ccli_print_color(interface, COLOR_YELLOW, "Usage: ./%s %s [OPTIONS]", interface->exeName, command->command);

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

static void ccli_command_display(ccli *interface, ccli_command *command) {
  ccli_print_color(interface, COLOR_YELLOW, "%s", command->command);
  if (command->description) {
    ccli_print_color(interface, COLOR_YELLOW, " -> %s", command->description);
  }
  ccli_print(interface, "\n");
}

static void ccli_display_commands(ccli *interface) {
  ccli_echo_color(interface, COLOR_YELLOW, "Commands:");
  for (int i = 0; i < interface->commands.size; i++) {
    ccli_print(interface, "  ");
    ccli_command_display(interface, interface->commands.commands[i]);
  }
}

static void ccli_usage(ccli *interface) {
  ccli_echo_color(interface, COLOR_YELLOW, "Usage: ./%s [command] [options]\n", interface->exeName);
}

static void ccli_display(ccli *interface) {

  ccli_usage(interface);

  if (interface->description) {
    ccli_echo_color(interface, COLOR_YELLOW, "  %s\n", interface->description);
  }

  // TODO: commands help
  ccli_display_commands(interface);
  ccli_print(interface, "\n");

  // TODO: global options help
}

/******************** ccli_arg retrieval ********************/

static void check_valid_arg_index(ccli *interface, int index) {
  if (!interface->invoked_command) {
    ccli_error(interface, "No command has been invoked yet.");
  } else if (interface->invoked_command->args.size <= index) {
    ccli_error(interface, "invalid arg index: max is %d, but you used %d.", interface->invoked_command->args.size - 1, index);
  }
}

int ccli_get_int_arg(ccli *interface, int index) {
  check_valid_arg_index(interface, index);

  ccli_value value = interface->invoked_command->args.args[index]->value;
  if (IS_NUM(value)) return AS_INT(value);
  else ccli_error(interface, "argument at index %d isn't a number.", index);
}

double ccli_get_double_arg(ccli *interface, int index) {
  check_valid_arg_index(interface, index);

  ccli_value value = interface->invoked_command->args.args[index]->value;
  if (IS_NUM(value)) return AS_DOUBLE(value);
  else ccli_error(interface, "argument at index %d isn't a number.", index);
}

bool ccli_get_bool_arg(ccli *interface, int index) {
  check_valid_arg_index(interface, index);

  ccli_value value = interface->invoked_command->args.args[index]->value;
  if (IS_BOOL(value)) return AS_BOOL(value);
  else ccli_error(interface, "argument at index %d isn't a boolean.", index);
}

char *ccli_get_string_arg(ccli *interface, int index) {
  check_valid_arg_index(interface, index);

  ccli_value value = interface->invoked_command->args.args[index]->value;
  if (IS_STRING(value)) return AS_STRING(value);
  else ccli_error(interface, "argument at index %d isn't a string.", index);
}

/******************** ccli global interface API ********************/


ccli_command *ccli_add_command(ccli *interface, char *command, ccli_command_callback callback) {
  ccli_command *_command = ccli_command_new(command, callback);
  ccli_command_add_option(_command, "--help", NULL, VAL_NULL);

  command_array_add(&interface->commands, _command);
  return _command;
}

void ccli_help(ccli *interface, ccli_command *command) {
  if (!command) {
    // global help
    ccli_display(interface);
    return;
  }

  ccli_command_display(interface, command);
}

static ccli_command *get_command(ccli *interface) {
  for (int i = 0; i < interface->commands.size; i++) {
    if (!strcmp(interface->argv[1], interface->commands.commands[i]->command)) {
      interface->current_arg++;
      return interface->commands.commands[i];
    }
  }

  return NULL;
}

typedef struct {
  char *name;
  char *val;
} parsed_option;

// these helpers are mainly for readability
#define parsed_option_new(arg, val) ((parsed_option){ arg, val })

static void parsed_option_free(parsed_option *option) {
  free(option->name);
}

static char *copy_chars(char *chars, int length) {
  char *string = malloc(sizeof(char) * (length + 1));
  strncpy(string, chars, length);
  string[length] = '\0';
  return string;
}

bool is_digit(char c) {
  return (c >= '0' && c <= '9');
}

bool is_number(char *value) {
  if (is_digit(value[0])) {
    return true;
  } else if (value[0] == '.') {
    return (strlen(value) > 1 && is_digit(value[1]));
  } else if (value[0] == '-') {
    return (strlen(value) > 1 && is_digit(value[1])) ||
           (strlen(value) > 2 && value[1] == '.' && is_digit(value[2]));
  } else return false;
}

bool is_bool(char *value) {
  return (!strcasecmp(value, "t") ||
          !strcasecmp(value, "f") ||
          !strcasecmp(value, "true") ||
          !strcasecmp(value, "false"));
}

// returns the boolean represented by [value].
// returns false if the value isn't valid.
bool strtobool(char *value) {
  return (!strcasecmp(value, "true") || !strcasecmp(value, "t"));
}

void set_option_value(ccli *interface, ccli_command *command, ccli_option *option, char *name, char *value) {
  if (!value) {
    if (option->type == VAL_NULL) {
      option->value = BOOL_VAL(true);
      return;
    } else {
      ccli_detailed_command_display(interface, command);
      ccli_error(interface, "missing option parameter: '%s'.", name);
    }
  }

  switch (option->type) {
    case VAL_NULL: {
      ccli_detailed_command_display(interface, command);
      ccli_error(interface, "option doesn't take parameter: '%s=%s'.", name, value);
    }
    case VAL_BOOL: {
      if (is_bool(value)) {
        option->value = BOOL_VAL(strtobool(value));
      } else {
        ccli_detailed_command_display(interface, command);
        ccli_error(interface, "invalid boolean: '%s'.", value);
      }
      break;
    }
    case VAL_NUM: {
      if (is_number(value)) {
        option->value = NUM_VAL(strtod(value, NULL));
      } else {
        ccli_detailed_command_display(interface, command);
        ccli_error(interface, "invalid number: '%s'.", value);
      }
      break;
    }
    case VAL_STRING: {
      option->value = STRING_VAL(value);
      break;
    }
    default:
      ccli_error(interface, "unrecognized value type: %d\n", option->type);
      // TODO: handle this more gracefully?
  }
}

parsed_option parse_option(char *arg) {
  if (arg[0] != '-') return parsed_option_new(NULL, NULL);

  int name_len = 0;

  // TODO: handle short options
  name_len++; name_len++;

  while (arg[name_len] != '\0' && arg[name_len] != '=') {
    name_len++;
  }

  if (arg[name_len] == '=') {
    return parsed_option_new(copy_chars(arg, name_len), &arg[name_len + 1]);
  } else {
    return parsed_option_new(copy_chars(arg, name_len), NULL);
  }
}

#undef parsed_option_new

void parse_options(ccli *interface, ccli_command *command) {
  parsed_option p_option;
  for (; interface->current_arg < interface->argc &&
         interface->argv[interface->current_arg][0] == '-';
         interface->current_arg++) {
    p_option = parse_option(interface->argv[interface->current_arg]);
    if (!p_option.name) return;

    ccli_option *option = NULL;
    // TODO: finish parsing options
    table_string *string = ccli_table_find_string(&command->options, p_option.name);
    if (string && ccli_table_get(&command->options, string, &option)) {
      // option was used
      set_option_value(interface, command, option, p_option.name, p_option.val);
    }

    parsed_option_free(&p_option);
  }
}

static void parse_arg(ccli *interface, ccli_command *command, ccli_arg *arg, char *value) {
  switch (arg->type) {
    case VAL_NUM: {
      if (is_number(value)) {
        arg->value = NUM_VAL(strtod(value, NULL));
      } else {
        ccli_detailed_command_display(interface, command);
        ccli_error(interface, "invalid number: '%s'.", value);
      }
      break;
    }
    case VAL_BOOL: {
      if (is_bool(value)) {
        arg->value = BOOL_VAL(strtobool(value));
      } else {
        ccli_detailed_command_display(interface, command);
        ccli_error(interface, "invalid boolean: '%s'.", value);
      }
      break;
    }
    case VAL_STRING: {
      arg->value = STRING_VAL(value);
      break;
    }
    default: {
      // Should be unreachable
      ccli_detailed_command_display(interface, command);
      ccli_error(interface, "invalid value type: %d.", arg->type);
      break;
    }
  }
}

static void parse_args(ccli *interface, ccli_command *command) {
  // short-circuit for commands with no arguments
  if (command->args.size == 0) return;

  int num_ccli_args = 0;
  for (; interface->current_arg < interface->argc && num_ccli_args < command->args.size;
         interface->current_arg++, num_ccli_args++) {
    ccli_arg *arg = command->args.args[num_ccli_args];
    char *value = interface->argv[interface->current_arg];
    parse_arg(interface, command, arg, value);
  }

  if (num_ccli_args < command->args.size) {
    // arguments are required
    ccli_detailed_command_display(interface, command);
    ccli_error(interface, "command requires %d arguments, but %d were specified.",
               command->args.size, num_ccli_args);
  }
}

void ccli_run(ccli *interface) {
  if (interface->argc <= 1 || !strcmp(interface->argv[interface->current_arg], "--help")) {
    ccli_help(interface, NULL);
    return;
  }

  ccli_command *command = get_command(interface);
  if (!command) {
    ccli_echo_color(interface, COLOR_RED, "Error: Unrecognized command -> '%s'\n", interface->argv[1]);
    ccli_display_commands(interface);
    ccli_print(interface, "\n");
    return;
  }

  interface->invoked_command = command;
  parse_options(interface, command);

  if (ccli_option_exists(interface, "--help")) {
    ccli_detailed_command_display(interface, interface->invoked_command);
    return;
  }

  parse_args(interface, command);

  command->callback(interface);
}