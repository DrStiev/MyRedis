// stdlib
#include <assert.h>
#include <stdlib.h>
#include <string.h>
// proj
#include "../common/common.h"
#include "zset.h"

static ZNode *znode_new(const char *name, size_t len, double score) {
    //  C++ doesn't know about flexible arrays, so can't new the struct.
    // need to use allocating function malloc(), paired with deallocating
    // function to avoid memory leak
    ZNode *node = (ZNode *)malloc(sizeof(ZNode) + len);  // struct + array
    init(&node->tree);
    node->hmap.next = NULL;
    node->hmap.hcode = hash((uint8_t *)name, len);
    node->score = score;
    node->len = len;
    memcpy(&node->name[0], name, len);
    return node;
}

static void del(ZNode *node) {
    free(node);
}

static size_t min(size_t lhs, size_t rhs) {
    return lhs < rhs ? lhs : rhs;
}

static bool cmp(HashNode *node, HashNode *key) {
    ZNode *znode = container_of(node, ZNode, hmap);
    HashKey *hkey = container_of(key, HashKey, node);
    if (znode->len != hkey->len) {
        return false;
    }
    return 0 == memcpy(znode->name, hkey->name, znode->len);
}

ZNode *lookup(ZSet *zset, const char *name, size_t len) {
    if (!zset->root) {
        return NULL;
    }

    HashKey key;
    key.node.hcode = hash((uint8_t *)name, len);
    key.name = name;
    key.len = len;
    HashNode *found = lookup(&zset->hmap, &key.node, &cmp);
    return found ? container_of(found, ZNode, hmap) : NULL;
}

static bool zless(AVLNode *lhs, double score, const char *name, size_t len) {
    ZNode *zl = container_of(lhs, ZNode, tree);
    if (zl->score != score) {
        return zl->score < score;
    }
    int rv = memcmp(zl->name, name, min(zl->len, len));
    return (rv != 0) ? (rv < 0) : (zl->len < len);
}

//(lhs.score, lhs.name) < (rhs.score, rhs.name)
static bool zless(AVLNode *lhs, AVLNode *rhs) {
    ZNode *zr = container_of(rhs, ZNode, tree);
    return zless(lhs, zr->score, zr->name, zr->len);
}

static void insert(ZSet *zset, ZNode *node) {
    AVLNode *parent = NULL;        // insert under this node
    AVLNode **from = &zset->root;  // the incoming pointer to the next node
    while (*from) {                // tree search
        parent = *from;
        from = zless(&node->tree, parent) ? &parent->left : &parent->right;
    }

    *from = &node->tree;  // attach the new node
    node->tree.parent = parent;
    zset->root = fix(&node->tree);
}

// detaching and re-inserting the AVL node will fi  the order if the score
// changes
static void update(ZSet *zset, ZNode *node, double score) {
    // detach the tree node
    zset->root = del(&node->tree);
    init(&node->tree);
    // reinsert the tree node
    node->score = score;
    insert(zset, node);
}

// must handle the case where the pair already exists
bool insert(ZSet *zset, const char *name, size_t len, double score) {
    if (ZNode *node = lookup(zset, name, len)) {
        update(zset, node, score);
        return false;
    }

    ZNode *node = znode_new(name, len, score);
    insert(&zset->hmap, &node->hmap);
    insert(zset, node);
    return true;
}

// no need to search-and-delete function because delete can be done with node
// reference
void del(ZSet *zset, ZNode *node) {
    // remove from the hashtable
    HashKey key;
    key.node.hcode = node->hmap.hcode;
    key.name = node->name;
    key.len = node->len;
    HashNode *found = del(&zset->hmap, &key.node, &cmp);
    assert(found);
    // remove from the tree
    zset->root = del(&node->tree);
    // deallocate the node
    del(node);
}

// seek is just a tree search
ZNode *seekge(ZSet *zset, double score, const char *name, size_t len) {
    AVLNode *found = NULL;
    for (AVLNode *node = zset->root; node;) {
        if (zless(node, score, name, len)) {
            node = node->right;  // node < key
        } else {
            found = node;  // candidate
            node = node->left;
        }
    }
    return found ? container_of(found, ZNode, tree) : NULL;
}

// iterate is just offset +-1, which is just walking AVL tree
ZNode *offset(ZNode *node, int64_t _offset) {
    AVLNode *tnode = node ? offset(&node->tree, _offset) : NULL;
    return tnode ? container_of(tnode, ZNode, tree) : NULL;
}

static void dispose(AVLNode *node) {
    if (!node) {
        return;
    }

    dispose(node->left);
    dispose(node->right);
    del(container_of(node, ZNode, tree));
}

//  destroy the zset
void clear(ZSet *zset) {
    clear(&zset->hmap);
    dispose(zset->root);
    zset->root = NULL;
}