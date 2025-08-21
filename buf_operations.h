#ifndef BUF_OPERATIONS_H
#define BUF_OPERATIONS_H

// stdlib
#include <stdint.h>
// C++
#include <vector>
#include <string>

// use std::vector as the buffer type, which is just a dynamic array
//append to the back
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data+len);
}

// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin()+n);
}

#endif