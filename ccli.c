#include "ccli.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

static void *reallocate(void *prev, size_t oldSize, size_t newSize) {
  if (!newSize) {
    free(prev);
    return NULL;
  }

  return realloc(prev, newSize);
}

typedef enum {
  OBJ_STRING,
  OBJ_COMMAND,
  OBJ_ARGUMENT,
  OBJ_OPTION
} obj_type;

typedef struct ccli_obj {
  obj_type type;
  struct ccli_obj *next;
} ccli_obj;

static ccli_obj *allocateObj(obj_type type, size_t size, ccli_obj *next) {
  ccli_obj *obj = reallocate(NULL, 0, size);
  obj->type = type;
  obj->next = next;
  return obj;
}

typedef struct {
  ccli_obj obj;
  char *chars;
  int length;
  uint32_t hash;
} ccli_string;

typedef enum {
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

#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

#define AS_NUM(value)  ((value).as.number)
#define AS_BOOL(value) ((value).as.boolean)
#define AS_OBJ(value)  ((value).as.obj)

#define NUM_VAL(value) ((ccli_value){ VAL_NUM, { .number = value } })
#define BOOL_VAL(value) ((ccli_value){ VAL_BOOL, { .boolean = value } })
#define OBJ_VAL(value) ((ccli_value){ VAL_OBJ, { .object = (ccli_obj *)value } })

/* TODO: ccli_table */

/* ccli - main interface */
struct ccli {
  FILE *fp;
  char *description;
  int argc;
  char **argv;
};

/******************** TESTING ********************/

#ifdef CCLI_TESTS

bool testPassed = true;
int passedTests = 0;
int totalTests = 0;

#define assert(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "    [%s:%d] Assert failed -> `%s`\n", \
          __FILE__, __LINE__, #condition); \
      testPassed = false; \
    } \
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

#define TEST(function) \
  do { \
    beginTest(#function); \
    function(); \
    endTest(#function); \
  } while (false)

static void valueTest() {
  ccli_value value = NUM_VAL(3);
  assert(IS_NUM(value));
  assert(AS_NUM(value) == 3);

  value = BOOL_VAL(true);
  assert(IS_BOOL(value));
  assert(AS_BOOL(value));
}

int main() {
  printf("begin tests\n\n");

  TEST(valueTest);
  
  printf("\nend tests -> %d/%d passed.\n", passedTests, totalTests);
}

#endif