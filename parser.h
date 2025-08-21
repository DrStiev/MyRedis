// stdlib
#include <string.h>
// proj
#include "types.h"
#include "buf_operations.h"
#include "hashtable.h"

#define container_of(ptr, T, member) \
    ((T *)( (char *)ptr - offsetof(T, member) ))

static bool eq(HashNode *lhs, HashNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

// helper function to deal with array indexes. This makes the code less error-prone
static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
    if (cur + 4 > end) {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

// remember *& is a reference to a pointer. References are just pointers with different syntax
static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(cur, cur + 4);
    cur += 4;
    return true;
}

/*
* A Redis request is a list of strings. Representing a list as a chunk of bytes is
* the task of (de)serialization. Using the same length-prefixed scheme as the outer message format.
* -----------------------------------------------------
* | nstr | len | str1 | len | str2 | ... | len | strn |
* -----------------------------------------------------
*    4B     4B    ...    4B   ...
*/
// Step 1: parse the request command. Length-prefixed data parsing (trivial)
static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out) {
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr)) {
        return  -1;
    }

    if (nstr > k_max_msg) {
        return -1; // safety limit
    }

    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(data, end, len)) {
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data, end, len, out.back())) {
            return -1;
        }
    }

    if (data != end) {
        return -1; // trailing garbage
    }

    return 0;
}

static void do_get(std::vector<std::string> &cmd, Response &out) {
    // a dummy 'Entry' just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());
    
    // hashable lookup
    HashNode *node =  lookup(&g_data.db, &key.node, &eq);
    if (!node) {
        out.status = RES_NX;
        return;
    }

    // copy the value
    const std::string &val = container_of(node, Entry, node)->val;
    assert(val.size() <= k_max_msg);
    out.data.assign(val.begin(), val.end());
}

static void do_set(std::vector<std::string> &cmd, Response &) {
    // dummy 'Entry' just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());

    //hashtable lookup
    HashNode *node = lookup(&g_data.db, &key.node, &eq);
    if (node) {
        // found, update the value
        container_of(node, Entry, node)->val.swap(cmd[2]);
    } else {
        // not found, allocate & insert a new pair
        Entry *ent =  new Entry();
        ent->key.swap(key.key);
        ent->node.hcode =  key.node.hcode;
        ent->val.swap(cmd[2]);
        insert(&g_data.db, &ent->node);
    }
}

static void do_del(std::vector<std::string> &cmd, Response &) {
    // dummy 'Entry' just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());
    //  hashable delete
    HashNode *node = hm_delete(&g_data.db, &key.node, &eq);
    if (node) {
        // deallocate the pair
        delete container_of(node, Entry, node);
    }
}

// Step 2: Process the command
static void do_request(std::vector<std::string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        return do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        do_del(cmd, out);
    } else {
        out.status = RES_ERR; // unrecognized command
    }
}

// Step 3: Serialize the response
static void make_response(const Response &resp, std::vector<uint8_t> &out) {
    uint32_t resp_len = 4 + (uint32_t)resp.data.size();
    buf_append(out, (const uint8_t *)&resp_len, 4);
    buf_append(out, (const uint8_t *)&resp.status, 4);
    buf_append(out, resp.data.data(), resp.data.size());
}
