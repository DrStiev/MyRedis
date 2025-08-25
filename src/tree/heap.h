#pragma once

// stdlib
#include <stddef.h>
#include <stdint.h>

// A binary tree is some dynamically allocated nodes linked by pointers.
// However btrees can be represented without pointers using an array
// Nodes are flattened into an array level by level. This requires that each
// level is fully filled. Array-encoded heap need 2 invariant:
// 1. a node's value is less than both its children
// 2. each level is fully filled except for the last
//
// Being array-encoded means no dynamic allocations, so insert and delete are
// faster
struct HeapItem {
    uint64_t val;  // expiration time
    size_t *ref;   // points to 'Entry::heap_idx'
};

void update(HeapItem *a, size_t pos, size_t len);