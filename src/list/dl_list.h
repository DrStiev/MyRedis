#pragma once

#include <stddef.h>
#include <stdint.h>

struct DL_List {
    DL_List *prev = NULL;
    DL_List *next = NULL;
};

// Dummy node is linked to itself, forming a circle
inline void init(DL_List *node) { node->prev = node->next = node; }

// due to the dummy node, insertion does not need to handle the empty case
inline void insert_before(DL_List *target, DL_List *rookie) {
    DL_List *prev = target->prev;
    prev->next = rookie;
    rookie->prev = prev;
    rookie->next = target;
    target->prev = rookie;
}

inline void detach(DL_List *node) {
    DL_List *prev = node->prev;
    DL_List *next = node->next;
    prev->next = next;
    next->prev = prev;
}

// and empty list is a list with only the dummy node
inline bool is_empty(DL_List *node) { return node->next == node; }