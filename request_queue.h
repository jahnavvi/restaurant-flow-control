#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include <pthread.h>
#include <semaphore.h>
#include "seating.h"

// Forward declare the queue type
typedef struct request_queue request_queue_t;

//structure representing the bounded request queue along with
//synchronization primitives and accounting information.
struct request_queue {
    int capacity;                 // maximum number of requests in the queue
    RequestType *buffer;          // ring buffer to hold request types
    int front;                    // index of the next item to remove
    int rear;                     // index of the next free slot to insert
    int count;                    // current number of items in the queue

    unsigned int produced_counts[RequestTypeN];
    unsigned int consumed_counts[ConsumerTypeN][RequestTypeN];
    unsigned int in_queue_counts[RequestTypeN];

    int total_produced;           // total number of requests produced
    int total_consumed;           // total number of requests consumed
    int total_limit;              // total number of requests to be produced
    int vip_limit;                // maximum number of VIP requests allowed in queue

    // Synchronization primitives
    pthread_mutex_t mutex; // Core lock for all queue access
    pthread_cond_t not_full; // Producers wait on
    pthread_cond_t not_empty; // Consumers wait on

    //
    //   Barrier semaphore used by the main thread to wait for all
    //   requests to be consumed.  Once the last request has been
    //   removed from the queue and all production is complete, a consumer
    //   signals this semaphore.
    //
    sem_t barrier; // Signals main when all consumed
    int barrier_signaled; // Ensure only once

    int producers_left; // How many producers still alive
    int producers_finished; // Set to 1 when all done
};

int init_request_queue(request_queue_t *rq, int capacity, int vip_limit, int total_limit);

//Destroy a previously initialised request queue, freeing the ring buffer
//and releasing any system resources (mutexes, condition variables and
// semaphores) held by the queue.
void destroy_request_queue(request_queue_t *rq);

// Insert a new request of the specified type into the queue.
void add_request(request_queue_t *rq, RequestType type);

// Remove the next request from the queue (FIFO order).
int remove_request(request_queue_t *rq, ConsumerType consumer, RequestType *type_out);

void mark_producer_finished(request_queue_t *rq);

#endif
