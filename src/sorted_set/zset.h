#pragma once

#include "../hashtable/hashtable.h"
#include "../tree/avl.h"

// A sorted set is a collection of sorted (score, name) pairs indexed in 2 ways

struct ZSet {
    AVLNode *root = NULL;  // index by (score, name)
    HashMap hmap;          // index by name
};

struct ZNode {
    // data structure nodes
    AVLNode tree;
    HashNode hmap;
    // data
    double score = 0;
    size_t len = 0;
    // used to embed the string into the node to reduce memory allocations
    char name[0];  // flexible array
};

// point queries and updates
bool insert(ZSet *zset, const char *name, size_t len, double score);
ZNode *lookup(ZSet *zset, const char *name, size_t len);
void del(ZSet *zset, ZNode *node);
// range queries command
ZNode *seekge(ZSet *zset, double score, const char *name, size_t len);
ZNode *offset(ZNode *node, int64_t _offset);
void clear(ZSet *zset);