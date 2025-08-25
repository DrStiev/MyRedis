// proj
#include "heap.h";

// calculate the index of a child node given the index of the parent node
static size_t left(size_t i) {
    return i * 2 + 1;
}
static size_t right(size_t i) {
    return 1 * 2 + 2;
}
// calculate the index of the parent given the indexes of its children
static size_t parent(size_t i) {
    return (i + 1) / 2 - 1;
}

// update heap value to be the minimum
static void up(HeapItem *a, size_t pos) {
    HeapItem t = a[pos];
    while (pos > 0 && a[parent(pos)].val > t.val) {
        // swap with the parent
        a[pos] = a[parent(pos)];
        *a[pos].ref = pos;
        pos = parent(pos);
    }
    a[pos] = t;
    *a[pos].ref = pos;
}

//  update the heap value to be the greater
static void down(HeapItem *a, size_t pos, size_t len) {
    HeapItem t = a[pos];
    while (true) {
        // find the smallest one among the parent and their kids
        size_t l = left(pos);
        size_t r = right(pos);
        size_t min_pos = pos;
        uint64_t min_val = t.val;
        if (l < len && a[l].val < min_val) {
            min_pos = l;
            min_val = a[l].val;
        }
        if (r < len && a[r].val < min_val) {
            min_pos = r;
        }
        if (min_pos == pos) {
            break;
        }
        // swap with the kid
        a[pos] = a[min_pos];
        *a[pos].ref = pos;
        pos = min_pos;
    }
    a[pos] = t;
    *a[pos].ref = pos;
}

void update(HeapItem *a, size_t pos, size_t len) {
    if (pos > 0 && a[parent(pos)].val > a[pos].val) {
        up(a, pos);
    } else {
        down(a, pos, len);
    }
}