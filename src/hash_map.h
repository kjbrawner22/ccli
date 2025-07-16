#pragma once

#include <stdbool.h>

#include "value.h"

typedef struct CcliHashMap CcliHashMap;

void ccli_hash_map_init(CcliHashMap* map);
void ccli_hash_map_free(CcliHashMap* map);

bool ccli_hash_map_set(CcliHashMap* map, const char* key, CcliValue value);
bool ccli_hash_map_get(CcliHashMap* map, const char* key, CcliValue* value);