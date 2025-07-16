#include "hash_map.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// tuning
#define TABLE_MAX_LOAD 0.75
#define GROW_ARRAY_CAPACITY(cap) ((cap == 0) ? 8 : (cap) * 2)

typedef struct {
  char* chars;
  uint32_t hash;
} HashString;

typedef struct {
  HashString* key;
  CcliValue value;
} HashMapEntry;

static uint32_t hash_string(const char* key)
{
  uint32_t hash = 2166136261u;

  for (int i = 0; i < strlen(key); i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

static HashString* hash_string_new(char* chars)
{
  HashString* string = malloc(sizeof(HashString));
  string->chars = chars;
  string->hash = hash_string(chars);
  return string;
}

struct CcliHashMap {
  HashMapEntry* entries;
  int count;
  int capacity;
};

void ccli_hash_map_init(CcliHashMap* map)
{
  map->capacity = 0;
  map->count = 0;
  map->entries = NULL;
}

void ccli_hash_map_free(CcliHashMap* table)
{
  for (int i = 0; i < table->capacity; i++) {
    HashString* string = table->entries[i].key;
    CcliValue* value = table->entries[i].value;
    if (string) free(string);
    if (value) free(value);
  }

  free(table->entries);
  ccli_hash_map_init(table);
}

// find an entry or its respective spot in the table
static HashMapEntry* ccli_table_find_entry(HashMapEntry* entries, int capacity, HashString* key)
{
  if (!entries) return NULL;

  uint32_t index = key->hash % capacity;
  HashMapEntry* tombstone = NULL;

  for (;;) {
    HashMapEntry* entry = &entries[index];

    if (!entry->key) {
      if (!entry->value) {
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

static void ccli_table_adjust_capacity(ccli_table* table, int capacity)
{
  HashMapEntry* entries = malloc(sizeof(HashMapEntry) * capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].option = NULL;
  }

  // don't copy over tombstones, reset and reconstruct the table
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    HashMapEntry* entry = &table->entries[i];

    // disregard tombstones and empty slots
    if (!entry->key) continue;

    HashMapEntry* dest = ccli_table_find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->option = entry->option;
    table->count++;
  }

  free(table->entries);

  table->entries = entries;
  table->capacity = capacity;
}

bool ccli_table_get(ccli_table* table, HashString* key, ccli_option** option)
{
  if (!table->entries) return false;

  HashMapEntry* entry = ccli_table_find_entry(table->entries, table->capacity, key);
  if (!entry->key) return false;

  *option = entry->option;
  return true;
}

static bool ccli_table_set(ccli_table* table, HashString* key, ccli_option* option)
{
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_ARRAY_CAPACITY(table->capacity);
    ccli_table_adjust_capacity(table, capacity);
  }

  HashMapEntry* entry = ccli_table_find_entry(table->entries, table->capacity, key);

  bool isNewKey = (entry->key == NULL);
  // increment count if it isn't a real value or a tombstone
  if (isNewKey && !entry->option) table->count++;

  entry->key = key;
  entry->option = option;
  return isNewKey;
}