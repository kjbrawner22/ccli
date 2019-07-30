#include "ccli.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

typedef enum {
  OBJ_STRING,
  OBJ_COMMAND,
  OBJ_ARGUMENT,
  OBJ_OPTION
} obj_type;

typedef struct ccli_obj {
  obj_type type;
} ccli_obj;

#define ALLOCATE_OBJ(objectType, type) \
  (type *)allocateObj(objectType, sizeof(type))
#define FREE_OBJ(obj, objectType) \
  freeObj(obj, objectType)

static ccli_obj *allocateObj(obj_type type, size_t size) {
  ccli_obj *obj = reallocate(NULL, 0, size);
  obj->type = type;
  return obj;
}

static void freeObj(void *obj, obj_type type) {
  switch (type) {
    case OBJ_STRING: free(obj);
    default:
      printf("Object type not implemented yet.\n");
      exit(1);
  }
}

typedef struct {
  ccli_obj obj;
  char *chars;
  int length;
  uint32_t hash;
} ccli_string;

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

typedef enum {
  VAL_NULL,
  VAL_NUM,
  VAL_BOOL,
  VAL_OBJ
} value_type;

typedef struct {
  value_type type;
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
#define AS_OBJ(value)  ((value).as.obj)

#define NULL_VAL        ((ccli_value){ VAL_NULL, { .number = 0                 } })
#define NUM_VAL(value)  ((ccli_value){ VAL_NUM,  { .number = value             } })
#define BOOL_VAL(value) ((ccli_value){ VAL_BOOL, { .boolean = value            } })
#define OBJ_VAL(value)  ((ccli_value){ VAL_OBJ,  { .object = (ccli_obj *)value } })

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

    ccli_value value = entry->value;
    if (value.type == VAL_OBJ) {
      FREE_OBJ(value.as.object, value.type);
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
    if (entry->key == NULL) continue;

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
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

bool setTable(ccli_table *table, ccli_string *key, ccli_value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  table_entry *entry = findEntry(table->entries, table->capacity, key);

  bool isNewKey = (entry->key == NULL);
  // Only increment the count if the entry isn't a tombstone
  if (isNewKey) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

ccli_string *tableFindString(ccli_table *table, const char *chars, int length, uint32_t hash) {
  if (table->entries == NULL) return NULL;

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
  FILE *fp;
  char *description;
  int argc;
  char **argv;
  ccli_table strings;
  ccli_obj *objects;
};

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
}

static void tableTest() {
  ccli_table table;
  initTable(&table);
  assert(!table.capacity);

  ccli_string *string = allocateString("hello", 5, hashString("hello", 5));
  ccli_value value = NULL_VAL;

  setTable(&table, string, NUM_VAL(21));
  assert(getTable(&table, string, &value));
  assert(AS_NUM(value) == 21);
  assert(tableFindString(&table, "hello", 5, string->hash));

  freeTable(&table);
}

int main() {
  printf("begin tests\n\n");

  TEST(valueTest);
  TEST(tableTest);
  
  printf("\nend tests -> %d/%d passed.\n", passedTests, totalTests);
}

#endif