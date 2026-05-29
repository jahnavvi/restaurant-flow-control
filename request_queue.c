#include "request_queue.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

// Helper initializes mutexes/condvars/semaphore
static int init_sync_primitives(request_queue_t *rq) {
    if (pthread_mutex_init(&rq->mutex, NULL) != 0) {
        return -1; // If mutex fails, nothing else to clean
    }
    if (pthread_cond_init(&rq->not_full, NULL) != 0) {
        pthread_mutex_destroy(&rq->mutex);
        return -1; // Cleanup mutex and return failure
    }
    if (pthread_cond_init(&rq->not_empty, NULL) != 0) {
        pthread_cond_destroy(&rq->not_full);
        pthread_mutex_destroy(&rq->mutex);
        return -1; // Cleanup and fail
    }
    if (sem_init(&rq->barrier, 0, 0) != 0) { // Semaphore starts at 0
        pthread_cond_destroy(&rq->not_empty);
        pthread_cond_destroy(&rq->not_full);
        pthread_mutex_destroy(&rq->mutex);
        return -1;
    }
    return 0;
}

// Initialize the queue structure
int init_request_queue(request_queue_t *rq, int capacity, int vip_limit, int total_limit) {
    if (!rq || capacity <= 0 || vip_limit < 0 || total_limit < 0) {
        return -1; // Validate arguments
    }

    memset(rq, 0, sizeof(*rq)); // Zero all fields initially

    rq->capacity = capacity; // Set general limits
    rq->vip_limit = vip_limit;
    rq->total_limit = total_limit;

    rq->buffer = (RequestType *)malloc(sizeof(RequestType) * capacity);
    if (!rq->buffer) {
        return -1; // Memory allocation failure
    }

    rq->front = rq->rear = 0; // Setup circular buffer indices
    rq->count = 0;

    // Initialize mutex, condvars, semaphores
    if (init_sync_primitives(rq) != 0) {
        free(rq->buffer);
        rq->buffer = NULL;
        return -1;
    }

    // Zero out producer and consumer counters
    for (int i = 0; i < RequestTypeN; i++) {
        rq->produced_counts[i] = 0;
        rq->in_queue_counts[i] = 0;
        for (int j = 0; j < ConsumerTypeN; j++) {
            rq->consumed_counts[j][i] = 0;
        }
    }

    rq->total_produced = 0; // Tracking totals
    rq->total_consumed = 0;

    return 0;
}

// Free memory + destroy locks
void destroy_request_queue(request_queue_t *rq) {
    if (!rq) return;
    free(rq->buffer);
    pthread_mutex_destroy(&rq->mutex);
    pthread_cond_destroy(&rq->not_full);
    pthread_cond_destroy(&rq->not_empty);
    sem_destroy(&rq->barrier);
}

// Add request into queue
void add_request(request_queue_t *rq, RequestType type) {
    if (!rq) return;

    pthread_mutex_lock(&rq->mutex); // Enter critical section

    // Block if queue full OR VIP limit exceeded
    while (rq->count >= rq->capacity || (type == VIPRoom && rq->in_queue_counts[VIPRoom] >= rq->vip_limit)) {
        pthread_cond_wait(&rq->not_full, &rq->mutex);
    }

    // Recheck production limit to avoid race
    if (rq->total_produced >= rq->total_limit) {
        pthread_mutex_unlock(&rq->mutex);
        return;
    }

    rq->buffer[rq->rear] = type; // Insert request
    rq->rear = (rq->rear + 1) % rq->capacity;
    rq->count++;
    rq->in_queue_counts[type]++;
    rq->produced_counts[type]++;
    rq->total_produced++;

    output_request_added(type, rq->produced_counts, rq->in_queue_counts); // Logging

    pthread_cond_signal(&rq->not_empty); // Wake waiting consumer
    pthread_mutex_unlock(&rq->mutex);
}

// Remove request for a consumer
int remove_request(request_queue_t *rq, ConsumerType consumer, RequestType *type_out) {
    if (!rq || !type_out) return -1;

    pthread_mutex_lock(&rq->mutex);

    while (rq->count == 0) {
        if (rq->producers_finished) {
            if (!rq->barrier_signaled) {
                rq->barrier_signaled = 1;
                sem_post(&rq->barrier); // Wake main thread
            }
            pthread_mutex_unlock(&rq->mutex);
            return -1;
        }
        pthread_cond_wait(&rq->not_empty, &rq->mutex);
    }

    RequestType t = rq->buffer[rq->front];
    rq->front = (rq->front + 1) % rq->capacity;
    rq->count--;
    rq->in_queue_counts[t]--;
    rq->consumed_counts[consumer][t]++;
    rq->total_consumed++;
    *type_out = t; // Give removed type back to caller

    output_request_removed(consumer, t, rq->consumed_counts[consumer], rq->in_queue_counts);

    if (!rq->barrier_signaled && rq->producers_finished && rq->count == 0) {
        rq->barrier_signaled = 1;
        sem_post(&rq->barrier); // Signal main
    }

    pthread_cond_broadcast(&rq->not_full); // Wake all waiting producers
    pthread_mutex_unlock(&rq->mutex);
    return 0;
}

// Called by a producer when it finishes
void mark_producer_finished(request_queue_t *rq) {
    if (!rq) return;
    pthread_mutex_lock(&rq->mutex);
    if (rq->producers_left > 0) {
        rq->producers_left--;
    }
    if (rq->producers_left == 0) {
        rq->producers_finished = 1;
        pthread_cond_broadcast(&rq->not_empty); // Wake consumers
    }
    pthread_mutex_unlock(&rq->mutex);
}
