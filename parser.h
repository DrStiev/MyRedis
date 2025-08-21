// stdlib
#include <string.h>

#include "types.h"
#include "buf_operations.h"

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

// Step 2: Process the command
static void do_request(std::vector<std::string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        auto it = g_data.find(cmd[1]);
        if (it == g_data.end()) {
            out.status = RES_NX; // not found
            return;
        }

        const std::string &val = it->second;
        out.data.assign(val.begin(), val.end());
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        g_data[cmd[1]].swap(cmd[2]);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        g_data.erase(cmd[1]);
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
