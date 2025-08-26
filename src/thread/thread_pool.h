#pragma once
// stdlib
#include <pthread.h>
#include <stddef.h>
// C++
#include <deque>
#include <vector>

// A thread pool has a fixed number of consumer threads, called "workers". An
// unspecified number of producers can issue tasks to workers via a queue.
// Consumers sleep when the queue is empty, until they are woken up by a
// producer.

struct Work {
    // A task is just a function pointer with a void* argument
    void (*f)(void *) = NULL;
    void *arg = NULL;
};

struct ThreadPool {
    std::vector<pthread_t> threads;
    std::deque<Work> queue;
    pthread_mutex_t mu;
    pthread_cond_t not_empty;
};

// The consumers (workers)
void init(ThreadPool *tp, size_t num_threads);
// The producer (event loop)
void queue(ThreadPool *tp, void (*f)(void *), void *arg);
