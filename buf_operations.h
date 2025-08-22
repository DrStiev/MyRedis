#ifndef BUF_OPERATIONS_H
#define BUF_OPERATIONS_H

// stdlib
#include <stdint.h>
// C++
#include <string>
#include <vector>
// proj
#include "types.h"

// use std::vector as the buffer type, which is just a dynamic array
// append to the back
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data,
                       size_t len) {
    buf.insert(buf.end(), data, data + len);
}

// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

// helper function to handle different endianness
static void buf_append_u8(Buffer &buf, uint8_t data) {
    buf.push_back(data);
}

// helper function to handle different endianness
static void buf_append_u32(Buffer &buf, uint32_t data) {
    buf_append(buf, (const uint8_t *)&data, 4);  // assume little endian
}

static void buf_append_i64(Buffer &buf, int64_t data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}

static void buf_append_dbl(Buffer &buf, double data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}

#endif