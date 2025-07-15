#pragma once

#include <stdbool.h>

typedef struct CcliHashMap CcliHashMap;

void ccli_hash_map_init(CcliHashMap* table);
void ccli_hash_map_free(CcliHashMap* table);

bool ccli_hash_map_set(CcliHashMap* table, const char* key, void* option);
bool ccli_hash_map_get(CcliHashMap* table, const char* key, void** option);