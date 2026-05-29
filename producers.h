#ifndef PRODUCERS_H
#define PRODUCERS_H

#include "request_queue.h" // Queue definitions
#include "seating.h" // RequestType enum

// Struct passed into producer threads
// Contains the queue pointer, the request type, and a sleep time delay
typedef struct {
    request_queue_t *queue; // Shared queue pointer used by all threads
    RequestType type; // GeneralTable or VIPRoom
    int sleep_ms; // Time spent "producing" each request
} producer_args_t;

// Producer thread entry point
void *producer_thread(void *arg);

#endif
