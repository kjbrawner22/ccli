#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ccli.h"

#define GROW_ARRAY_CAPACITY(cap) ((cap == 0) ? 8 : (cap) * 2)

/******************** ccli_value ********************/

struct ccli_value {
  ccli_value_type type;
  union {
    double number;
    bool boolean;
    char *string;
  } as;
};

#define NULL_VAL          ((ccli_value){ VAL_NULL,   { .number = 0 } })
#define BOOL_VAL(value)   ((ccli_value){ VAL_BOOL,   { .boolean = value } })
#define NUM_VAL(value)    ((ccli_value){ VAL_NUM,    { .number = (double)value } })
#define STRING_VAL(value) ((ccli_value){ VAL_STRING, { .string = value }})

#define IS_NULL(value)  ((value).type == VAL_NULL)

ccli_value *ccli_int(int value) {
  ccli_value *_value = malloc(sizeof(ccli_value));
  *_value = NUM_VAL(value);
  return _value;
}

ccli_value *ccli_bool(bool value) {
  ccli_value *_value = malloc(sizeof(ccli_value));
  *_value = BOOL_VAL(value);
  return _value;
}

ccli_value *ccli_string(char *string) {
  ccli_value *_value = malloc(sizeof(ccli_value));
  *_value = STRING_VAL(string);
  return _value;
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

struct ccli_table {
  table_entry *entries;
  int count;
  int capacity;
};

void ccli_table_init(ccli_table *table) {
  table->capacity = 0;
  table->count = 0;
  table->entries = NULL;
}

void ccli_table_free(ccli_table *table) {
  if (table->entries) {
    free(table->entries);
  }

  ccli_table_init(table);
}

// find an entry or its respective spot in the table
static table_entry *ccli_table_find_entry(table_entry *entries, int capacity, table_string *key) {
  if (entries == NULL) return NULL;

  uint32_t index = key->hash % capacity;
  table_entry *tombstone = NULL;

  for (;;) {
    table_entry *entry = &entries[index];

    if (entry->key == NULL) {
      if (IS_NULL(entry->option->value)) {
        // empty entry, return tombstone entry if found
        return (tombstone != NULL) ? tombstone : entry;
      } else {
        // found a tombstone
        if (tombstone == NULL) tombstone = entry;
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
    entries[i].option->value = NULL_VAL;
  }

  // don't copy over tombstones, reset and reconstruct the table
  table->count = 0;
  for (int i = 0; i < capacity; i++) {
    table_entry *entry = &table->entries[i];

    // disregard tombstones and empty slots
    if (entry->key == NULL) continue;

    table_entry *dest = ccli_table_find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->option = entry->option;
    table->count++;
  }

  free(table->entries);
  table->entries = entries;
  table->capacity = capacity;
}

bool ccli_table_get(ccli_table *table, table_string *key, ccli_option *option) {
  if (table->entries == NULL) return false;

  table_entry *entry = ccli_table_find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  option = entry->option;
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
  if (isNewKey && IS_NULL(entry->option->value)) table->count++;

  entry->key = key;
  entry->option = option;
  return isNewKey;
}

static table_string *ccli_table_find_string(ccli_table *table, const char *chars) {
  if (table->entries == NULL) return NULL;

  uint32_t hash = hash_string(chars);

  uint32_t index = hash % table->capacity;

  for (;;) {
    table_entry *entry = &table->entries[index];
    if (entry->key == NULL) {
      // stop if we find an empty, non-tombstone entry
      if (IS_NULL(entry->option->value)) return NULL;
    } else if (!strcmp(entry->key->chars, chars)) {
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

/******************** option_array ********************/
/*
typedef struct {
  int size;
  int capacity;
  ccli_option **options;
} option_array;

static void option_array_init(option_array *array) {
  array->capacity = 0;
  array->size = 0;
  array->options = NULL;
}

static void option_array_free(option_array *array) {
  if (array->capacity > 0) {
    for (int i = 0; i < array->size; i++) {
      free(array->options[i]);
    }

    free(array->options);
  }

  option_array_init(array);
}

static void option_array_add(option_array *array, ccli_option *option) {
  if (array->size + 1 > array->capacity) {
    array->capacity = GROW_ARRAY_CAPACITY(array->capacity);
    array->options = realloc(array->options, sizeof(ccli_option *) * array->capacity);
  }

  array->options[array->size++] = option;
}
*/
/******************** ccli_command ********************/

struct ccli_command {
  char *command;
  char *description;
  ccli_command_callback callback;
  ccli_table options;
};

static ccli_command *ccli_command_new(char *command, ccli_command_callback callback) {
  ccli_command *_command = malloc(sizeof(ccli_command));
  _command->command = command;
  _command->description = NULL;
  _command->callback = callback;
  ccli_table_init(&_command->options);
  return _command;
}

static void ccli_command_free(ccli_command *command) {
  ccli_table_free(&command->options);
  free(command);
}

void ccli_command_set_description(ccli_command *command, char *description) {
  command->description = description;
}

ccli_option *ccli_command_add_option(ccli_command *command, char *double_dash_option, 
                             char *single_dash_option, ccli_value_type type) {
  //TODO: implement global options
  if (command == NULL || 
     (double_dash_option == NULL && single_dash_option == NULL)) return NULL;
  
  ccli_option *option = ccli_option_new(double_dash_option, single_dash_option, 
                                        type ? type : VAL_BOOL);
  
  table_string *string;
  if (double_dash_option != NULL) {
    string = ccli_table_find_string(&command->options, double_dash_option);
    ccli_table_set(&command->options, string ? string : table_string_new(double_dash_option), option);
  }

  if (single_dash_option != NULL) {
    string = ccli_table_find_string(&command->options, single_dash_option);
    ccli_table_set(&command->options, string ? string : table_string_new(single_dash_option), option);
  }
  
  return option;
}

/********** command_array **********/

#define MIN_ARRAY_CAPACITY 8

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
  if (array->capacity > 0) {
    for (int i = 0; i < array->size; i++) {
      ccli_command_free(array->commands[i]);
    }

    free(array->commands);
  }

  command_array_init(array);
}

static void command_array_add(command_array *array, ccli_command *command) {
  if (array->size + 1 > array->capacity) {
    array->capacity = GROW_ARRAY_CAPACITY(array->capacity);
    array->commands = realloc(array->commands, sizeof(ccli_command *) * array->capacity);
  }

  array->commands[array->size++] = command;
}

#undef MIN_ARRAY_CAPACITY

/******************** ccli ********************/

struct ccli {
  char *exeName;
  int argc;
  char **argv;
  char *description;
  FILE *fp;
  command_array commands;
};

ccli *ccli_init(char *exeName, int argc, char **argv) {
  ccli *interface = malloc(sizeof(ccli));
  interface->exeName = exeName;
  interface->argc = argc;
  interface->argv = argv;
  interface->description = NULL;
  interface->fp = stdout;
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

ccli_command *ccli_add_command(ccli *interface, char *command, ccli_command_callback callback) {
  ccli_command *_command = ccli_command_new(command, callback);
  command_array_add(&interface->commands, _command);
  return _command;
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
    fputc('\n', stdout);
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

static void ccli_command_display(ccli *interface, ccli_command *command) {
  ccli_print_color(interface, COLOR_YELLOW, "%s", command->command);
  if (command->description) {
    ccli_print_color(interface, COLOR_YELLOW, " -> %s\n", command->description);
  }
}

static void ccli_display_commands(ccli *interface) {
  ccli_echo_color(interface, COLOR_YELLOW, "Commands:");
  for (int i = 0; i < interface->commands.size; i++) {
    ccli_print(interface, "  ");
    ccli_command_display(interface, interface->commands.commands[i]);
  }
}

static void ccli_display(ccli *interface) {

  ccli_echo_color(interface, COLOR_YELLOW, "Usage: ./%s [command] [options]\n", interface->exeName);

  if (interface->description) {
    ccli_echo_color(interface, COLOR_YELLOW, "  %s\n", interface->description);
  }

  // TODO: commands help
  ccli_display_commands(interface);
  ccli_print(interface, "\n");

  // TODO: options help
}

/******************** ccli global interface API ********************/

void ccli_help(ccli *interface, ccli_command *command) {
  if (command == NULL) {
    // global help
    ccli_display(interface);
    return;
  }

  ccli_command_display(interface, command);
}

static ccli_command *get_command(ccli *interface) {
  for (int i = 0; i < interface->commands.size; i++) {
    if (!strcmp(interface->argv[1], interface->commands.commands[i]->command)) {
      return interface->commands.commands[i];
    }
  }

  return NULL;
}


void ccli_run(ccli *interface) {
  if (interface->argc <= 1 || !strcmp(interface->argv[1], "--help")) {
    ccli_help(interface, NULL);
    return;
  }

  ccli_command *command = get_command(interface);
  if (command == NULL) {
    ccli_echo_color(interface, COLOR_RED, "Error: Unrecognized command -> '%s'\n", interface->argv[1]);
    ccli_display_commands(interface);
    ccli_print(interface, "\n");
    return;
  }

  command->callback(interface);
}