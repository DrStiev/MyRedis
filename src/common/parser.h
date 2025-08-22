#pragma once
// stdlib
#include <string.h>
// proj
#include "buf_operations.h"
#include "../hashtable/hashtable.h"
#include "types.h"

// de factor standard for intrusive data structures.
#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

static bool eq(HashNode *lhs, HashNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

// function to output serialized data
static void out_nil(Buffer &out) {
    buf_append_u8(out, TAG_NIL);
}

// function to output serialized data
static void out_str(Buffer &out, const char *s, size_t size) {
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t)size);
    buf_append(out, (const uint8_t *)s, size);
}

// function to output serialized data
static void out_int(Buffer &out, int64_t val) {
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}

// function to output serialized data
static void out_dbl(Buffer &out, double val) {
    buf_append_u8(out, TAG_DBL);
    buf_append_dbl(out, val);
}

// function to output serialized data
static void out_arr(Buffer &out, uint32_t n) {
    buf_append_u8(out, TAG_ARR);
    buf_append_u32(out, n);
}

// function to output serialized data
static void out_err(Buffer &out, uint32_t code, const std::string &msg) {
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    buf_append(out, (const uint8_t *)msg.data(), msg.size());
}

// helper function to deal with array indexes. This makes the code less
// error-prone
static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
    if (cur + 4 > end) {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

// remember *& is a reference to a pointer. References are just pointers with
// different syntax
static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n,
                     std::string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(cur, cur + 4);
    cur += 4;
    return true;
}

/*
 * A Redis request is a list of strings. Representing a list as a chunk of bytes
 * is the task of (de)serialization. Using the same length-prefixed scheme as
 * the outer message format.
 * +------+-----+------+-----+------+-----+-----+------+
 * | nstr | len | str1 | len | str2 | ... | len | strn |
 * +------+-----+------+-----+------+-----+-----+------+
 *    4B     4B    ...    4B   ...
 */
// Step 1: parse the request command. Length-prefixed data parsing (trivial)
static int32_t parse_req(const uint8_t *data, size_t size,
                         std::vector<std::string> &out) {
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr)) {
        return -1;
    }

    if (nstr > k_max_msg) {
        return -1;  // safety limit
    }

    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(data, end, len)) {
            return -1;
        }
        out.push_back(std::string());
        if (!read_str(data, end, len, out.back())) {
            return -1;
        }
    }

    if (data != end) {
        return -1;  // trailing garbage
    }

    return 0;
}

static void do_get(std::vector<std::string> &cmd, Buffer &out) {
    // a dummy 'Entry' just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());

    // hashable lookup
    HashNode *node = lookup(&g_data.db, &key.node, &eq);
    if (!node) {
        return out_nil(out);
    }

    // copy the value
    const std::string &val = container_of(node, Entry, node)->val;
    return out_str(out, val.data(), val.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out) {
    // dummy 'Entry' just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());

    // hashtable lookup
    HashNode *node = lookup(&g_data.db, &key.node, &eq);
    if (node) {
        // found, update the value
        container_of(node, Entry, node)->val.swap(cmd[2]);
    } else {
        // not found, allocate & insert a new pair
        Entry *ent = new Entry();
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->val.swap(cmd[2]);
        insert(&g_data.db, &ent->node);
    }
    return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out) {
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
    return out_int(out, node ? 1 : 0);
}

static bool cb_keys(HashNode *node, void *arg) {
    Buffer &out = *(Buffer *)arg;
    const std::string &key = container_of(node, Entry, node)->key;
    out_str(out, key.data(), key.size());
    return true;
}

static void do_keys(std::vector<std::string> &, Buffer &out) {
    out_arr(out, (uint32_t)size(&g_data.db));
    foreach (&g_data.db, &cb_keys, (void *)&out)
        ;
}

// Step 2: Process the command
static void do_request(std::vector<std::string> &cmd, Buffer &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        return do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        return do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        return do_del(cmd, out);
    } else if (cmd.size() == 1 && cmd[0] == "keys") {
        return do_keys(cmd, out);
    } else {
        return out_err(out, ERR_UNKNOWN, "unknown command");
    }
}

// Step 3: Serialize the response
static void response_begin(Buffer &out, size_t *header) {
    *header = out.size();    // message header position
    buf_append_u32(out, 0);  // reserve space
}

static size_t response_size(Buffer &out, size_t header) {
    return out.size() - header - 4;
}

static void response_end(Buffer &out, size_t header) {
    size_t msg_size = response_size(out, header);
    if (msg_size > k_max_msg) {
        out.resize(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big");
        msg_size = response_size(out, header);
    }
    // message header
    uint32_t len = (uint32_t)msg_size;
    memcpy(&out[header], &len, 4);
}