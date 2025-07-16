#pragma once

#include <stdbool.h>

typedef enum {
  CCLI_VAL_NULL,
  CCLI_VAL_NUM,
  CCLI_VAL_BOOL,
  CCLI_VAL_STRING,
} CcliValueType;

typedef struct {
  CcliValueType type;
  union {
    double number;
    bool boolean;
    char* string;
  } as;
} CcliValue;

#define NULL_VAL ((ccli_value){CCLI_VAL_NULL, {.number = 0}})
#define BOOL_VAL(value) ((ccli_value){CCLI_VAL_BOOL, {.boolean = value}})
#define NUM_VAL(value) ((ccli_value){CCLI_VAL_NUM, {.number = (double)value}})
#define STRING_VAL(value) ((ccli_value){CCLI_VAL_STRING, {.string = value}})

#define IS_NULL(value) ((value).type == CCLI_VAL_NULL)
#define IS_NUM(value) ((value).type == CCLI_VAL_NUM)
#define IS_BOOL(value) ((value).type == CCLI_VAL_BOOL)
#define IS_STRING(value) ((value).type == CCLI_VAL_STRING)

#define AS_INT(value) ((int)((value).as.number))
#define AS_DOUBLE(value) ((value).as.number)
#define AS_BOOL(value) ((value).as.boolean)
#define AS_STRING(value) ((value).as.string)