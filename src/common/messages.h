#pragma once

// stdlib
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// need a way to print out formatted messages
inline void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

// need a way to print out formatted error messages
inline void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

// need a way to print out messages and abort execution
inline void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}
