#include "hash_map.h"

// tuning
#define TABLE_MAX_LOAD 0.75
#define GROW_ARRAY_CAPACITY(cap) ((cap == 0) ? 8 : (cap) * 2)

typedef struct {
  char* chars;
  uint32_t hash;
} table_string;

typedef struct {
  table_string* key;
  void* option;
} table_entry;

uint32_t hash_string(const char* key)
{
  uint32_t hash = 2166136261u;

  for (int i = 0; i < strlen(key); i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

table_string* table_string_new(char* chars)
{
  table_string* string = malloc(sizeof(table_string));
  string->chars = chars;
  string->hash = hash_string(chars);
  return string;
}

typedef struct {
  table_entry* entries;
  int count;
  int capacity;
} ccli_table;

void ccli_table_init(ccli_table* table)
{
  table->capacity = 0;
  table->count = 0;
  table->entries = NULL;
}

void ccli_table_free(ccli_table* table)
{
  for (int i = 0; i < table->capacity; i++) {
    table_string* string = table->entries[i].key;
    ccli_option* option = table->entries[i].option;
    if (string) free(string);
    if (option) free(option);
  }

  free(table->entries);
  ccli_table_init(table);
}

// find an entry or its respective spot in the table
static table_entry* ccli_table_find_entry(table_entry* entries, int capacity, table_string* key)
{
  if (!entries) return NULL;

  uint32_t index = key->hash % capacity;
  table_entry* tombstone = NULL;

  for (;;) {
    table_entry* entry = &entries[index];

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

static void ccli_table_adjust_capacity(ccli_table* table, int capacity)
{
  table_entry* entries = malloc(sizeof(table_entry) * capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].option = NULL;
  }

  // don't copy over tombstones, reset and reconstruct the table
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    table_entry* entry = &table->entries[i];

    // disregard tombstones and empty slots
    if (!entry->key) continue;

    table_entry* dest = ccli_table_find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->option = entry->option;
    table->count++;
  }

  free(table->entries);

  table->entries = entries;
  table->capacity = capacity;
}

bool ccli_table_get(ccli_table* table, table_string* key, ccli_option** option)
{
  if (!table->entries) return false;

  table_entry* entry = ccli_table_find_entry(table->entries, table->capacity, key);
  if (!entry->key) return false;

  *option = entry->option;
  return true;
}

static bool ccli_table_set(ccli_table* table, table_string* key, ccli_option* option)
{
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_ARRAY_CAPACITY(table->capacity);
    ccli_table_adjust_capacity(table, capacity);
  }

  table_entry* entry = ccli_table_find_entry(table->entries, table->capacity, key);

  bool isNewKey = (entry->key == NULL);
  // increment count if it isn't a real value or a tombstone
  if (isNewKey && !entry->option) table->count++;

  entry->key = key;
  entry->option = option;
  return isNewKey;
}