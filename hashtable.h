#ifndef HASHTABLE_H
#define HASHTABLE_H

#pragma once

#include <stddef.h>
#include <stdint.h>

// Fixed-size chaining hashtable
// For a chaining  hashtable, the load factor limit should be greater than 1
const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128; // constant work

// Step 0: Choose a hash function. For Redis do not use cryptographic hash functions for hashtables because they are slow and overkill

// Step 1: Define the intrusive list node. Just an intrusove linked list node with the hash value of the key
struct HashNode {
    HashNode *next = NULL;
    uint64_t hcode = 0;
}; 

/*
* Step 2: Define the fixed-size hashtable.
* hash(key) % N maps a hash value to a slot, but Modulo or division are slow CPU operations
* is common to use powers of 2 so it can be done by a fast bitwise and: hash(key) & (N-1)
*/ 
struct HashTable {
    HashNode **table = NULL; // array of slots
    size_t mask = 0; // power of 2 array size, 2**n-1
    size_t size = 0; // number of keys
};

// Step 8: Define hashtable interfaces
struct HashMap {
    HashTable newer;
    HashTable older;
    size_t migrate_pos = 0;
};

// generic functions
HashNode *lookup(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *));
void insert(HashMap *hmap, HashNode *node);
HashNode *hm_delete(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *));
void clear(HashMap *hmap);
size_t size(HashMap *hmap);

// FNV hash
static uint64_t hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

#endif