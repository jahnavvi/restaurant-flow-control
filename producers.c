#include "producers.h" // Producer function declarations + arg struct
#include <unistd.h> // usleep() for simulating delays

// Entry point for each producer thread
void *producer_thread(void *arg) {
    producer_args_t *parg = (producer_args_t *)arg; // Cast from void* to real type
    request_queue_t *rq = parg->queue; // Shared queue pointer
    RequestType type = parg->type; // Whether this producer is General or VIP
    int sleep_ms = parg->sleep_ms; // Delay between requests

    while (1) {
        // Simulate production time before inserting into the queue
        if (sleep_ms > 0) {
            usleep((useconds_t)sleep_ms * 1000); // Convert ms -> microseconds
        }

        // Lock the mutex briefly to check production count safely
        pthread_mutex_lock(&rq->mutex);
        if (rq->total_produced >= rq->total_limit) {
            pthread_mutex_unlock(&rq->mutex); // Release the lock if limit reached
            break; // Exit the loop -> producer is done
        }
        pthread_mutex_unlock(&rq->mutex); // Release after reading

        // Insert request into queue (function handles full queue / VIP limit)
        add_request(rq, type);
    }

    // Tell queue system that this producer is finished
    mark_producer_finished(rq);

    return NULL; // Standard pthread exit
}
