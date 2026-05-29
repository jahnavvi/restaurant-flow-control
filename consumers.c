#include "consumers.h" // Consumer thread definitions
#include <unistd.h> // usleep() for delays

// Entry point for consumer threads
void *consumer_thread(void *arg) {
    consumer_args_t *carg = (consumer_args_t *)arg; // Convert to strong type
    request_queue_t *rq = carg->queue; // Pointer to shared queue
    ConsumerType consumer = carg->consumer; // TX or Rev9
    int sleep_ms = carg->sleep_ms; // Processing delay

    while (1) {
        RequestType t; // Will hold request removed from queue

        // Try to remove a request. If -1 -> nothing left -> exit thread
        int ret = remove_request(rq, consumer, &t);
        if (ret < 0) {
            break; // Exit loop -> consumer is done
        }

        // Simulate processing time outside critical section
        if (sleep_ms > 0) {
            usleep((useconds_t)sleep_ms * 1000);
        }
    }

    return NULL; // Thread exits
}
