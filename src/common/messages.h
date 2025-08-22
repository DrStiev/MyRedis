#pragma once

// stdlib
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// need a way to print out formatted messages
static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

// need a way to print out formatted error messages
static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

// need a way to print out messages and abort execution
static void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}
