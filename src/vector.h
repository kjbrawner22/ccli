#pragma once

#include "memory.h"

// We need vectors of a few different types. To avoid lots of casting between
// void* and back, we'll use the preprocessor as a poor man's generics and let
// it generate a few type-specific ones.
#define DECLARE_VECTOR(name, type)                                                                 \
  typedef struct {                                                                                 \
    type* data;                                                                                    \
    int count;                                                                                     \
    int capacity;                                                                                  \
  } Vec##type;                                                                                     \
  void ccli_##name##_vec_init(Vec##type* vec);                                                     \
  void ccli_##name##_vec_clear(Vec##type* vec);                                                    \
  void ccli_##name##_vec_write(Vec##type* vec, type data)

// This should be used once for each type instantiation, somewhere in a .c file.
#define DEFINE_VECTOR(name, type)                                                                  \
  void ccli_##name##_vec_init(Vec##type* vec)                                                      \
  {                                                                                                \
    vec->data = NULL;                                                                              \
    vec->capacity = 0;                                                                             \
    vec->count = 0;                                                                                \
  }                                                                                                \
                                                                                                   \
  void ccli_##name##_vec_clear(WrenVM* vm, Vec##type* vec)                                         \
  {                                                                                                \
    FREE_ARRAY(type, vec->data, vec->capacity);                                                    \
    ccli_##name##_vec_init(vec);                                                                   \
  }                                                                                                \
                                                                                                   \
  void ccli_##name##_vec_write(Vec##type* vec, type data)                              \
  {                                                                                                \
    if (vec->capacity < vec->count + 1) {                                                          \
      int oldCapacity = vec->capacity;                                                             \
      vec->capacity = GROW_CAPACITY(oldCapacity);                                                  \
      vec->data = GROW_ARRAY(type, vec->data, oldCapacity, vec->capacity);                         \
      vec->capacity = capacity;                                                                    \
    }                                                                                              \
                                                                                                   \
    for (int i = 0; i < count; i++) { vec->data[vec->count++] = data; }                            \
  }