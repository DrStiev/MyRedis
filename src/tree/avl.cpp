#include <assert.h>
// proj
#include "avl.h"

static uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

static void update(AVLNode *node) {
    node->height = 1 + max(height(node->left), height(node->right));
    node->count = 1 + count(node->left) + count(node->right);
}

/*
 * Step 2: Rotations
 * Rotations change the shape of a subtree while keeping its order
 * Binary tree rotation is used in AVL tree. It can change the tree height while
 * keeping the data ordered.
 */
static AVLNode *rote_left(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->right;
    AVLNode *inner = node->left;

    // node <-> inner
    node->right = inner;
    if (inner) {
        inner->parent = node;
    }

    // parent <- new_node
    new_node->parent = parent;  // NOTE: may be NULL

    // new_node <-> node
    new_node->left = node;
    node->parent = new_node;

    // auxiliary data
    update(node);
    update(new_node);
    return new_node;
}

static AVLNode *rote_right(AVLNode *node) {
    AVLNode *parent = node->parent;
    AVLNode *new_node = node->left;
    AVLNode *inner = node->right;

    // node <-> inner
    node->left = inner;
    if (inner) {
        inner->parent = node;
    }

    // parent <- new_node
    new_node->parent = parent;  // NOTE: may be NULL

    // new_node <-> node
    new_node->right = node;
    node->parent = new_node;

    // auxiliary data
    update(node);
    update(new_node);
    return new_node;
}

// Step 3: Fix height difference of 2
static AVLNode *fix_left(AVLNode *node) {  // left is too tall
    if (height(node->left->left) < height(node->left->right)) {
        node->left = rote_left(node->left);  // Transformation 2
    }
    return rote_right(node);  // Transformation 1
}

static AVLNode *fix_right(AVLNode *node) {  // right is too tall
    if (height(node->right->right) < height(node->right->left)) {
        node->right = rote_right(node->right);  // Transformation 1
    }
    return rote_left(node);  // Transformation 2
}

// Step 4: Fix imbalances ater an insert/delete
AVLNode *fix(AVLNode *node) {
    while (true) {
        AVLNode **from = &node;  // save the fixed subtree here
        AVLNode *parent = node->parent;
        if (parent) {
            // attach the fixed subtree to the parent
            from = parent->left == node ? &parent->left : &parent->right;
        }  // else: save to the local variable 'node'

        // auxuuary data
        update(node);

        // fix the height difference of 2
        uint32_t l = height(node->left);
        uint32_t r = height(node->right);
        if (l == r + 2) {
            *from = fix_left(node);
        } else if (l + 2 == r) {
            *from = fix_right(node);
        }

        // root node, stop
        if (!parent) {
            return *from;
        }

        // continue to the parent node because its height may be changed
        node = parent;
    }
}

// Step 5: Detach a node (easy case)
static AVLNode *del_easy(AVLNode *node) {
    assert(!node->left || !node->right);                     // at most 1 child
    AVLNode *child = node->left ? node->left : node->right;  // can be NULL
    AVLNode *parent = node->parent;

    // update the child's parent pointer
    if (child) {
        child->parent = parent;  // can be NULL
    }

    // attach the child to the grandparent
    if (!parent) {
        return child;  //  removing the root node
    }

    AVLNode **from = parent->left == node ? &parent->left : &parent->right;
    *from = child;
    // rebalance the updated tree
    return fix(parent);
}

// Step 6: Detach a node (hard case)
AVLNode *del(AVLNode *node) {
    // the easy case of 0 or 1 child
    if (!node->left || !node->right) {
        return del_easy(node);
    }

    // find successor
    AVLNode *victim = node->right;
    while (victim->left) {
        victim = victim->left;
    }

    // detach the successor
    AVLNode *root = del_easy(victim);

    // swap with the successor
    *victim = *node;  // left, right, parent
    if (victim->left) {
        victim->left->parent = victim;
    }
    if (victim->right) {
        victim->right->parent = victim;
    }

    // attach the sccessor to the parent, or update the root pointer
    AVLNode **from = &root;
    AVLNode *parent = node->parent;
    if (parent) {
        from = parent->left == node ? &parent->left : &parent->right;
    }

    *from = victim;
    return root;
}

// offset into the succeeding or preceding node
// note: the worst-case is O(logN) regardless of  how long the offset is
AVLNode *offset(AVLNode *node, int64_t offset) {
    int64_t pos = 0;  // the rank difference from the starting node
    while (offset != pos) {
        if (pos < offset && pos + count(node->right) >= offset) {
            // the target is inside the right subtree
            node = node->right;
            pos += count(node->left) + 1;
        } else if (pos > offset && pos - count(node->left) <= offset) {
            // the target is  inside the left subtree
            node = node->left;
            pos -= count(node->right) + 1;
        } else {
            // go to the parent
            AVLNode *parent = node->parent;
            if (!parent) {
                return NULL;
            }
            if (parent->right == node) {
                pos -= count(node->left) + 1;
            } else {
                pos += count(node->right) + 1;
            }
            node = parent;
        }
    }
    return node;
}