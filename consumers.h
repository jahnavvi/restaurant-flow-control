#ifndef CONSUMERS_H
#define CONSUMERS_H

#include "request_queue.h" // Queue operations
#include "seating.h" // ConsumerType enum

// Struct for passing params to a consumer thread
typedef struct {
    request_queue_t *queue; // Pointer to shared queue
    ConsumerType consumer; // TX or Rev9
    int sleep_ms; // Delay after consuming each request
} consumer_args_t;

// Entry point function for consumers
void *consumer_thread(void *arg);

#endif
