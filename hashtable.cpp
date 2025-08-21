// stdlib
#include <assert.h>
#include <stdlib.h>
// proj
#include "hashtable.h"

static void init(HashTable *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0); // n must be a power of 2 
    htab->table = (HashNode **)calloc(n, sizeof(HashNode *));
    htab->mask = n - 1;
    htab->size = 0;
}

// Step 3: Linked list insertion
static void insert(HashTable *htab, HashNode *node) {
    size_t pos = node->hcode & htab->mask; // node ->hcode & (n-1)
    HashNode *next = htab->table[pos];
    // Insert new node at the front, so insertion is O(1). Being intrusive means that there's no allocation in the data structure code
    node->next = next;
    htab->table[pos] = node;
    htab->size++;
}

// Step 6: Hashtable lookup
static HashNode **lookup(HashTable *htab, HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    if (!htab->table) {
        return NULL;
    }

    size_t pos =  key->hcode & htab->mask;
    HashNode **from = &htab->table[pos]; // incoming pointer to target
    for (HashNode *cur; (cur = *from) != NULL; from =  &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            // we need the address of the pointer to delete it
            return from; // return  from instead of cur for deletion
        }
    }
    return NULL;
}

// Step 7: Hashtable deletion
static HashNode *detach(HashTable *htab, HashNode **from) {
    HashNode *node = *from; // the target node
    *from = node->next; // update the incoming pointer to target
    htab->size--;
    return node;
}

/*
* Step 9: Deal with 2 hashtables during rehashing.
* Normally, HashMap::newer is used while HashMap::older is not. 
* But during rehashing, lookup or delet may need to query both tables
*/ 
static void trigger_rehashing(HashMap *hmap) {
    hmap->older = hmap->newer; // (newer, older) <- (new_table, newer)
    init(&hmap->newer, (hmap->newer.mask+1) * 2);
    hmap->migrate_pos = 0;
}

static void help_rehashing(HashMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_rehashing_work && hmap->older.size >  0) {
        //  find a non-empty slot
        HashNode **from = &hmap->older.table[hmap->migrate_pos];
        if (!*from) {
            hmap->migrate_pos++;
            continue; // empty slot
        }
        // move the first list itemto the newer table
        insert(&hmap->newer, detach(&hmap->older, from));
        nwork++;
    }
    // discard the old table if done
    if (hmap->older.size == 0 && hmap->older.table) {
        free(hmap->older.table);
        hmap->older = HashTable();
    }
}

HashNode *lookup(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    help_rehashing(hmap);
    HashNode **from = lookup(&hmap->newer, key, eq);
    if (!from) {
        from = lookup(&hmap->older, key, eq);
    }
    return from ? *from : NULL;
}

HashNode *hm_delete(HashMap *hmap, HashNode *key, bool (*eq)(HashNode *, HashNode *)) {
    help_rehashing(hmap);
    if (HashNode **from = lookup(&hmap->newer, key, eq)) {
        return detach(&hmap->newer, from);
    }
    if (HashNode **from = lookup(&hmap->older, key, eq)) {
        return detach(&hmap->older, from);
    }
    return NULL;
}

// Step 10: Trigger rehashing by the load factor. Insertion always update the newer table. It triggers rehashing when the load factor is high
void insert(HashMap *hmap, HashNode *node) {
    if (!hmap->newer.table) {
        init(&hmap->newer, 4); // initialized if empty
    }
    insert(&hmap->newer, node); // always insert to the newer table
    if (!hmap->older.table) { // check wheter we need to rehash
        size_t threshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= threshold) {
            trigger_rehashing(hmap);
        }
    }
    help_rehashing(hmap); // migrate some keys
}

void clear(HashMap *hmap) {
    free(hmap->newer.table);
    free(hmap->older.table);
    *hmap =  HashMap();
}

size_t size(HashMap *hmap) {
    return hmap->newer.size + hmap->older.size;
}