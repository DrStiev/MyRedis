// stdlib
#include <assert.h>
// proj
#include "../src/tree/avl.h"
#include "../src/common/common.h"

struct Data {
    AVLNode node;
    uint32_t val = 0;
};

struct Container {
    AVLNode *root = NULL;
};

static void add(Container &c, uint32_t val) {
    Data *data = new Data();
    init(&data->node);
    data->val = val;

    if (!c.root) {
        c.root = &data->node;
        return;
    }

    AVLNode *cur = c.root;
    while (true) {
        AVLNode **from = (val < container_of(cur, Data, node)->val)
                             ? &cur->left
                             : &cur->right;
        if (!*from) {
            *from = &data->node;
            data->node.parent = cur;
            c.root = fix(&data->node);
        }
        cur = *from;
    }
}

static void dispose(AVLNode *node) {
    if (node) {
        dispose(node->left);
        dispose(node->right);
        delete container_of(node, Data, node);
    }
}

static void test_case(uint32_t sz) {
    Container c;
    for (uint32_t i = 0; i < sz; ++i) {
        add(c, i);
    }

    AVLNode *min = c.root;
    while (min->left) {
        min = min->left;
    }

    for (uint32_t i = 0; i < sz; ++i) {
        AVLNode *node = offset(min, (int64_t)i);
        assert(container_of(node, Data, node)->val == i);

        for (uint32_t j = 0; j < sz; ++j) {
            int64_t _offset = (int64_t)j - (int64_t)i;
            AVLNode *n2 = offset(node, _offset);
            assert(container_of(n2, Data, node)->val == j);
        }
        assert(!offset(node, -(int64_t)i - 1));
        assert(!offset(node, sz - i));
    }
    dispose(c.root);
}

int main() {
    for (uint32_t i = 1; i < 500; ++i) {
        test_case(i);
    }
    return 0;
}