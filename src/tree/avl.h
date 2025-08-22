#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * AVL Tree: the heights of the 2 subtrees can only differ by 1 at most
 * +----------+------------+--------+--------+-----------+
 * |   Tree   | Worst case | Branch | Random | Difficuly |
 * +----------+------------+--------+--------+-----------+
 * | AVL Tree |   O(log N) |    2   |   No   |   Medium  |
 * +----------+------------+--------+--------+-----------+
 */

// Step 1: Auxiliary data in the tree node
// Struct Node {Node *left = NULL; Node *right = NULL;};
struct AVLNode {
    AVLNode *parent = NULL;
    AVLNode *left = NULL;
    AVLNode *right = NULL;
    uint32_t height = 0;  // auxiliary data for AVL tree
};

inline void init(AVLNode *node) {
    node->left = node->right = node->parent = NULL;
    node->height = 1;
}

// helpers
inline uint32_t height(AVLNode *node) {
    return node ? node->height : 0;
}


// API
AVLNode *fix(AVLNode *node);
AVLNode *del(AVLNode *node);
