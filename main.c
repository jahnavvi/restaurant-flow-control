#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "seating.h"
#include "request_queue.h"
#include "producers.h"
#include "consumers.h"
#include "log.h"

#define DEFAULT_TOTAL_REQUESTS 110 // Default number of requests to generate
#define DEFAULT_QUEUE_CAPACITY 20 // Max queue size
#define DEFAULT_VIP_LIMIT 6 // Max VIP requests allowed in queue at once

// Prints the usage message when command-line arguments are wrong
static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [-s total_requests] [-x ms] [-r ms] [-g ms] [-v ms]\n"
            "  -s N  Total number of requests to generate (default: %d)\n"
            "  -x N  Average processing time in ms for T-X consumer (default: 0)\n"
            "  -r N  Average processing time in ms for Rev-9 consumer (default: 0)\n"
            "  -g N  Average production time in ms for general table producer (default: 0)\n"
            "  -v N  Average production time in ms for VIP room producer (default: 0)\n",
            prog, DEFAULT_TOTAL_REQUESTS);
}

int main(int argc, char *argv[]) {
    int opt; // Used to parse command-line options
    int total_requests = DEFAULT_TOTAL_REQUESTS; // Initialize defaults
    int tx_sleep = 0;
    int rev9_sleep = 0;
    int general_sleep = 0;
    int vip_sleep = 0;

    // Parse flags using getopt
    while ((opt = getopt(argc, argv, "s:x:r:g:v:")) != -1) {
        switch (opt) {
            case 's':
                total_requests = atoi(optarg);
                break;
            case 'x':
                tx_sleep = atoi(optarg);
                break;
            case 'r':
                rev9_sleep = atoi(optarg);
                break;
            case 'g':
                general_sleep = atoi(optarg);
                break;
            case 'v':
                vip_sleep = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    request_queue_t rq; // Declare the request queue object

    // Initialize the queue with capacity, VIP limit, and total request limit
    if (init_request_queue(&rq, DEFAULT_QUEUE_CAPACITY, DEFAULT_VIP_LIMIT, total_requests) != 0) {
        fprintf(stderr, "Failed to initialise request queue\n");
        return EXIT_FAILURE;
    }

    rq.producers_left = 2; // There are exactly 2 producers in this program

    // Build argument structs for threads
    producer_args_t prod_general = { .queue = &rq, .type = GeneralTable, .sleep_ms = general_sleep };
    producer_args_t prod_vip = { .queue = &rq, .type = VIPRoom, .sleep_ms = vip_sleep };
    consumer_args_t cons_tx = { .queue = &rq, .consumer = TX, .sleep_ms = tx_sleep };
    consumer_args_t cons_rev9 = { .queue = &rq, .consumer = Rev9, .sleep_ms = rev9_sleep };

    pthread_t prod_threads[2]; // Array to store producer thread handles
    pthread_t cons_threads[2]; // Array for consumer thread handles

    // Create producer threads in the required order (General first, VIP second)
    if (pthread_create(&prod_threads[0], NULL, producer_thread, &prod_general) != 0) {
        fprintf(stderr, "Error creating general producer thread\n");
        destroy_request_queue(&rq);
        return EXIT_FAILURE;
    }
    if (pthread_create(&prod_threads[1], NULL, producer_thread, &prod_vip) != 0) {
        fprintf(stderr, "Error creating VIP producer thread\n");
        pthread_cancel(prod_threads[0]); // Cancel already-running producer
        pthread_join(prod_threads[0], NULL);
        destroy_request_queue(&rq);
        return EXIT_FAILURE;
    }

    // Create consumer threads in required order: T-X then Rev-9
    if (pthread_create(&cons_threads[0], NULL, consumer_thread, &cons_tx) != 0) {
        fprintf(stderr, "Error creating T-X consumer thread\n");
        pthread_cancel(prod_threads[0]); // Cancel both producers
        pthread_cancel(prod_threads[1]);
        pthread_join(prod_threads[0], NULL);
        pthread_join(prod_threads[1], NULL);
        destroy_request_queue(&rq);
        return EXIT_FAILURE;
    }
    if (pthread_create(&cons_threads[1], NULL, consumer_thread, &cons_rev9) != 0) {
        fprintf(stderr, "Error creating Rev-9 consumer thread\n");
        pthread_cancel(cons_threads[0]); // Cancel T-X consumer
        pthread_cancel(prod_threads[0]); // Cancel producers
        pthread_cancel(prod_threads[1]);
        pthread_join(cons_threads[0], NULL);
        pthread_join(prod_threads[0], NULL);
        pthread_join(prod_threads[1], NULL);
        destroy_request_queue(&rq);
        return EXIT_FAILURE;
    }

    // Wait for all producers to finish producing
    pthread_join(prod_threads[0], NULL);
    pthread_join(prod_threads[1], NULL);

    // Wait until the very last consumer signals that the queue is drained
    sem_wait(&rq.barrier);

    // Join consumer threads
    pthread_join(cons_threads[0], NULL);
    pthread_join(cons_threads[1], NULL);

    // Build an array to pass into production history output
    unsigned int *consumed_ptrs[ConsumerTypeN];
    for (int c = 0; c < ConsumerTypeN; c++) {
        consumed_ptrs[c] = rq.consumed_counts[c]; // Point each consumer to its counters
    }

    // Output final summary of counts
    output_production_history(rq.produced_counts, consumed_ptrs);

    destroy_request_queue(&rq); // Cleanup memory + mutexes + cond vars
    return EXIT_SUCCESS;
}
