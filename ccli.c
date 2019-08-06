#include "ccli.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static void *reallocate(void *prev, size_t oldSize, size_t newSize) {
  if (!newSize) {
    free(prev);
    return NULL;
  }

  return realloc(prev, newSize);
}

#define ALLOCATE(type, count) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count))
#define GROW_ARRAY(previous, type, oldCount, count) \
  (type *)reallocate(previous, sizeof(type) * oldCount, \
  sizeof(type) * count)
#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * oldCount, 0)
#define FREE(type, pointer) \
  reallocate(pointer, sizeof(type), 0)

typedef enum {
  OBJ_STRING,
  OBJ_COMMAND,
  OBJ_ARGUMENT,
  OBJ_OPTION
} obj_type;

typedef struct {
  obj_type type;
} ccli_obj;

typedef enum {
  VAL_NULL,
  VAL_NUM,
  VAL_BOOL,
  VAL_OBJ
} ccli_value_type;

typedef struct {
  ccli_value_type type;
  union {
    double number;
    bool boolean;
    ccli_obj *object;
  } as;
} ccli_value;

#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

#define AS_NUM(value)  ((value).as.number)
#define AS_BOOL(value) ((value).as.boolean)
#define AS_OBJ(value)  ((value).as.object)

#define NULL_VAL        ((ccli_value){ VAL_NULL, { .number = 0                 } })
#define NUM_VAL(value)  ((ccli_value){ VAL_NUM,  { .number = value             } })
#define BOOL_VAL(value) ((ccli_value){ VAL_BOOL, { .boolean = value            } })
#define OBJ_VAL(value)  ((ccli_value){ VAL_OBJ,  { .object = (ccli_obj *)value } })

typedef struct {
  ccli_obj obj;
  char *chars;
  int length;
  uint32_t hash;
} ccli_string;

typedef struct {
  ccli_string *name;
  char *description;
  ccli_command_callback callback;
} ccli_command;

typedef struct {
  ccli_string *name;
  char *description;
  ccli_value_type type;
  ccli_value value;
} ccli_argument;

typedef struct {
  ccli_string *name;
  char *description;
  ccli_value_type type;
  ccli_value value;
} ccli_option;

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_COMMAND(value) isObjType(value, OBJ_COMMAND)
#define IS_STRING(value)  isObjType(value, OBJ_STRING)

#define AS_COMMAND(value) ((ccli_command *)AS_OBJ(value))
#define AS_STRING(value)  ((ccli_string *)AS_OBJ(value))
#define AS_CSTRING(value) (((ccli_string *)AS_OBJ(value))->chars)

static inline bool isObjType(ccli_value value, obj_type type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#define ALLOCATE_OBJ(objectType, type) \
  (type *)allocateObj(objectType, sizeof(type))

static ccli_obj *allocateObj(obj_type type, size_t size) {
  ccli_obj *obj = reallocate(NULL, 0, size);
  obj->type = type;
  return obj;
}

static void freeObj(ccli_obj *obj) {
  switch (obj->type) {
    case OBJ_STRING: {
      ccli_string *string = (ccli_string *)obj;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ccli_string, string);
      break;
    }
    case OBJ_COMMAND: {
      ccli_command *command = (ccli_command *)obj;
      FREE(ccli_command, command);
      break;
    }
    default:
      printf("Object type not implemented yet.\n");
      break;
  }
}

static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

static ccli_string *allocateString(char *chars, int length, 
                                   uint32_t hash) {
  ccli_string *string = ALLOCATE_OBJ(OBJ_STRING, ccli_string);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  return string;
}

static ccli_command *allocateCommand(ccli_string *name, ccli_command_callback callback) {
  ccli_command *command = ALLOCATE_OBJ(OBJ_COMMAND, ccli_command);
  command->name = name;
  command->description = NULL;
  command->callback = callback;
  command->description = NULL;
  return command;
}

static ccli_argument *allocateArgument(ccli_string *name, ccli_value_type type) {
  ccli_argument *argument = ALLOCATE_OBJ(OBJ_ARGUMENT, ccli_argument);
  argument->name = name;
  argument->description = NULL;
  argument->type = type;
  argument->value = NULL_VAL;
  return argument;
}

static ccli_option *allocateOption(ccli_string *name, ccli_value_type type) {
  ccli_option *option = ALLOCATE_OBJ(OBJ_OPTION, ccli_option);
  option->name = name;
  option->description = NULL;
  option->type = type;
  option->value = NULL_VAL;
  return option;
}

/******************** ccli_table ********************/

#define TABLE_MAX_LOAD 0.75

#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

typedef struct {
  ccli_string *key;
  ccli_value value;
} table_entry;

typedef struct {
  int count;
  int capacity;
  table_entry *entries;
} ccli_table;

static void initTable(ccli_table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

static void freeTable(ccli_table *table) {
  for (int i = 0; i < table->capacity; i++) {
    table_entry *entry = &table->entries[i];

    if (!entry->key) continue;

    freeObj((ccli_obj *)entry->key);

    ccli_value value = entry->value;
    if (IS_OBJ(value)) {
      freeObj(value.as.object);
    }
  }

  FREE_ARRAY(table_entry, table->entries, table->capacity);
  initTable(table);
}

static table_entry *findEntry(table_entry *entries, int capacity, ccli_string *key) {
  uint32_t index = key->hash % capacity;

  for (;;) {
    table_entry *entry = &entries[index];

    if (entry->key == NULL) {
      // empty entry
      return entry;
    } else if (entry->key == key) {
      // We found the key
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(ccli_table *table, int capacity) {
  table_entry *entries = ALLOCATE(table_entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries->key = NULL;
    entries->value = NULL_VAL;
  }

  for (int i = 0; i < table->capacity; i++) {
    table_entry *entry = &table->entries[i];
    // Disregard empty entries
    if (!entry->key) continue;

    table_entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
  }
  
  FREE_ARRAY(table_entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool getTable(ccli_table *table, ccli_string *key, ccli_value *value) {
  if (table->entries == NULL) return false;

  table_entry *entry = findEntry(table->entries, table->capacity, key);
  if (!entry->key) return false;

  *value = entry->value;
  return true;
}

bool setTable(ccli_table *table, ccli_string *key, ccli_value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  table_entry *entry = findEntry(table->entries, table->capacity, key);

  bool isNewKey = (!entry->key);
  // Only increment the count if the entry isn't a tombstone
  if (isNewKey) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

ccli_string *tableFindString(ccli_table *table, const char *chars, int length, uint32_t hash) {
  if (!table->entries) return NULL;

  uint32_t index = hash % table->capacity;

  for (;;) {
    table_entry *entry = &table->entries[index];

    if (!entry->key) {
      return NULL;
    } else if (entry->key->length == length && 
               entry->key->hash == hash &&
               memcmp(entry->key->chars, chars, length) == 0) {
      // Found the string
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

/******************** ccli - main interface ********************/

struct ccli {
  char *description;
  int argc;
  int currentArg;
  ccli_string **argv;
  ccli_table strings;
  ccli_table commands;
  ccli_command *invokedCommand;
};

static ccli_string *copyString(ccli *interface, char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ccli_string *interned = tableFindString(&interface->strings, chars, length, hash);
  if (interned) return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  
  ccli_string *string = allocateString(heapChars, length, hash);
  setTable(&interface->strings, string, NULL_VAL);
  return string;
}

ccli *newCCLI(int argc, char **argv) {
  ccli *interface = malloc(sizeof(ccli));
  interface->argc = argc;
  interface->currentArg = 1;
  interface->description = NULL;
  interface->invokedCommand = NULL;
  initTable(&interface->commands);
  initTable(&interface->strings);

  interface->argv = malloc(sizeof(ccli_string *) * argc);
  for (int i = 0; i < argc; i++) {
    interface->argv[i] = copyString(interface, argv[i], strlen(argv[i]));
  }

  return interface;
}

void freeCCLI(ccli *interface) {
  freeTable(&interface->strings);
  free(interface);
}

void setDescriptionCCLI(ccli *interface, char *description) {
  interface->description = description;
}

ccli_command *addCommandCCLI(ccli *interface, char *name, ccli_command_callback callback) {
  ccli_string *nameString = copyString(interface, name, strlen(name));
  ccli_command *command = allocateCommand(nameString, callback);
  setTable(&interface->commands, nameString, OBJ_VAL(command));
  return command;
}

/* main runCCLI entrypoint and helpers */

bool isAtEnd(ccli *interface) {
  return interface->currentArg >= interface->argc;
}

void printGlobalHelp(ccli *interface) {
  printf("Usage: %s <command> [options]\n\n", interface->argv[0]->chars);

  if (interface->description) {
    printf("  %s\n\n", interface->description);
  }
}

void parseCommand(ccli *interface) {
  ccli_string *key = interface->argv[interface->currentArg++];
  ccli_value value;
  if (getTable(&interface->commands, key, &value) && IS_COMMAND(value)) {
    interface->invokedCommand = AS_COMMAND(value);
    printf("found command\n");
  }
}

void runCCLI(ccli *interface) {
  if (interface->argc <= 0) {
    return;
  } else if (interface->argc == 1) {
    printGlobalHelp(interface);
  }

  while (!isAtEnd(interface)) {
    parseCommand(interface);
  }
}

/*************************************************/
/******************** TESTING ********************/
/*************************************************/

#ifdef CCLI_TESTS

bool testPassed = true;
int passedTests = 0;
int totalTests = 0;

#define assert(condition)                                    \
  do {                                                       \
    if (!(condition)) {                                      \
      fprintf(stderr, "    [%s:%d] Assert failed -> `%s`\n", \
          __FILE__, __LINE__, #condition);                   \
      testPassed = false;                                    \
    }                                                        \
  } while(0)

static void beginTest(const char *function) {
  printf("  begin %s...\n", function);
  testPassed = true;
}

static void endTest(const char *function) {
  if (testPassed) {
    printf("  %s passed\n", function);
    passedTests++;
  } else printf("  %s failed\n", function);
  
  totalTests++;
}

#define TEST(function)    \
  do {                    \
    beginTest(#function); \
    function();           \
    endTest(#function);   \
  } while (false)

/******************** Tests go here ********************/

static void valueTest() {
  ccli_value value = NUM_VAL(3);
  assert(IS_NUM(value));
  assert(AS_NUM(value) == 3);

  value = BOOL_VAL(true);
  assert(IS_BOOL(value));
  assert(AS_BOOL(value));

  value = NULL_VAL;
  assert(IS_NULL(value));
  assert(!value.as.number);
  
  char *chars = "test string.";
  int length = strlen(chars);
  ccli_string *string = copyString(chars, length);
  value = OBJ_VAL(string);
  assert(IS_OBJ(value));
  assert(value.as.object->type == OBJ_STRING);

  freeObj(value.as.object);
}

static void tableTest() {
  ccli_table table;
  initTable(&table);
  assert(!table.capacity);

  ccli_string *string = copyString("hello", 5);
  ccli_value value = NULL_VAL;

  setTable(&table, string, NUM_VAL(21));
  assert(getTable(&table, string, &value));
  assert(AS_NUM(value) == 21);
  assert(tableFindString(&table, "hello", 5, string->hash));

  string = copyString("hi", 2);
  setTable(&table, string, BOOL_VAL(true));
  assert(!strcmp(string->chars, "hi"));
  assert(getTable(&table, string, &value));
  assert(IS_BOOL(value));
  assert(AS_BOOL(value));

  assert(table.capacity == 8);
  assert(table.count == 2);

  ccli_string *string2 = copyString("not in table.", 13);
  assert(!getTable(&table, string2, &value));
  assert(IS_BOOL(value));
  assert(AS_BOOL(value));

  ccli_string *string3 = tableFindString(&table, string->chars, string->length, string->hash);

  assert(string == string3);

  freeTable(&table);

  assert(table.count == 0);
  assert(table.capacity == 0);
  assert(!table.entries);

  assert(!getTable(&table, string, &value));
}

int main() {
  printf("begin tests\n\n");

  TEST(valueTest);
  TEST(tableTest);
  
  printf("\nend tests -> %d/%d passed.\n", passedTests, totalTests);
}

#endif