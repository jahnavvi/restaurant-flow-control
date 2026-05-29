#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "log.h"

/*
 * i/o functions - assumed to be called in a critical section
 */

/* Handle C++ namespaces, ignore if compiled in C
 * C++ usually uses this #define to declare the C++ standard.
 * It will not be defined if a C compiler is used.
 */
#ifdef __cplusplus
using namespace std;
#endif

sem_t sem_print;  // Semaphore for ensuring mutex printing,
                  // I know this is bad practice in terms of using Global, which is not ideal.
                  // Do NOT use the Global variables in your code!

pthread_once_t init_once = PTHREAD_ONCE_INIT;  // Ensures one-time initialization

void initialize_semaphore() {
    sem_init(&sem_print, 0, 1);  // Initialize semaphore with value 1
}

/*
 * Data section - names must align with the enumerated types
 * defined in seating.h
 */

/* Names of producer threads and request types */
const char *producerNames[] = {"General table request", "VIP room request"};
const char *producerAbbrevs[] = {"GT", "VIP"};

/* Names of consumer threads */
const char *consumerNames[] = {"T-X", "REV-9"};

/* double elapsed_s()
 * show seconds of wall clock time used by the process
 */
double elapsed_s() {
  const double ns_per_s = 1e9; /* nanoseconds per second */

  /* Initialize the first time we call this */
  static struct timespec start;
  static int firsttime = 1;
  struct timespec t;

  /* get elapsed wall clock time for this process
   * note:  use CLOCK_PROCESS_CPUTIME_ID for CPU time (not relevant here
   * as we will be sleeping a lot on the semaphores)
   */
  clock_gettime(CLOCK_REALTIME, &t);

  if (firsttime) {
    /* first time we've called the function, store the current
     * time.  This will not track the cost of the first item
     * produced, but is a reasonable approximation for the
     * whole process and avoids having to create an initialization
     * function.
     * (In C++, we'd just build a timer object and pass it around,
     *  but this approximation provides a simple interface for both
     *  C and C++.)
     */
    firsttime = 0;  /* don't do this again */
    start = t;  /* note when we started */
  }

  /* determine time delta from start and convert to s */
  double s = (t.tv_sec - start.tv_sec) +
    (t.tv_nsec - start.tv_nsec) / ns_per_s ;
  return s;
}

/**
 * @brief Show that a request has been added to the request queue and
 *        print the current status of the request queue.
 *
 * @param type              What kind of request was produced?
 * @param produced          Array of number of requests of each type that have been
 *                          produced
 * @param inRequestQueue    Array of number of requests of each type that are
 *                          in the request queue and have not yet been consumed.
 *                          (inRequestQueue[GeneralTable] and inRequestQueue[VIPRoom])
 *
 * produced and inRequestQueue reflect numbers *after* adding the current request.
 */
void output_request_added(RequestType requestType,
                          unsigned int produced[],
                          unsigned int inRequestQueue[]) {
  int idx;
  int total;

  pthread_once(&init_once, initialize_semaphore);  // Ensure one-time initialization

  // lock for mutex printing
  sem_wait(&sem_print);

  /* Show what is in the request queue */
  printf("Request queue: ");
  total = 0;  /* total produced */
  for (idx=0; idx < RequestTypeN; idx++) {
    if (idx > 0)
      printf(" + ");  /* separator */
    printf("%d %s", inRequestQueue[idx], producerAbbrevs[idx]);
    total += inRequestQueue[idx];
  }
  printf(" = %d. ", total);
  printf("Added %s.", producerNames[requestType]);

  /* Show what has been produced */
  total = 0;
  printf(" Produced: ");
  for (idx=0; idx < RequestTypeN; idx++) {
    total += produced[idx];  /* track total produced */
    if (idx > 0)
      printf(" + ");  /* separator */
    printf("%d %s", produced[idx], producerAbbrevs[idx]);
  }

  /* total produced over how long */
  printf(" = %d in %.3f s.\n", total, elapsed_s());
  //printf(" = %d\n", total);

  /* This is not really needed, but will be helpful for making sure that you
   * see output prior to a segmentation vioilation.  This is not usually a
   * good practice as we want to avoid ending the CPU burst premaurely which
   * this will do, but it is a helpful technique.
   */
  fflush(stdout);
  sem_post(&sem_print); // release the semaphore for printing
};

/**
 * @brief   Show that an item has been removed from the request queue
 *          and print the current status of the request queue.
 * @param consumers         Who removed and processed the request?
 * @param type              What kind of request was removed?  As each consumer consumes
 *                          one type of request, this is the same as the producer.
 * @param consumed          Array of number of requests of each type that have been
 *                          consumed.
 * @param inRequestQueue    Array of number of requests of each type that are
 *                          in the request queue and have not yet been consumed.
 *                          (inRequestQueue[GeneralTable] and inRequestQueue[VIPRoom])
 *
 * Counts reflect numbers *after* the request has been removed
 */
void output_request_removed(Consumers consumer,
                            RequestType requestType,
                            unsigned int consumed[],
                            unsigned int inRequestQueue[]) {
  int idx;
  int total;

  pthread_once(&init_once, initialize_semaphore);  // Ensure one-time initialization

  // lock for mutex printing
  sem_wait(&sem_print);

  /* Show what is in the request queue */
  total = 0;
  printf("Request queue: ");
  for (idx=0; idx < RequestTypeN; idx++) {
    if (idx > 0)
      printf(" + ");  /* separator */
    printf("%d %s", inRequestQueue[idx], producerAbbrevs[idx]);
    total += inRequestQueue[idx];
  }
  printf(" = %d. ", total);

  /* Show what has been consumed by consumer */
  printf("%s consumed %s.  %s totals: ",
          consumerNames[consumer],
          producerNames[requestType],
          consumerNames[consumer]);
  total = 0;
  for (idx = 0; idx < RequestTypeN; idx++) {
    if (idx > 0)
      printf(" + ");  /* separator */
    total += consumed[idx];  /* track total consumed */
    printf("%d %s", consumed[idx], producerAbbrevs[idx]);
  }

  /* total consumed over how long */
  printf(" = %d consumed in %.3f s.\n", total, elapsed_s());
  //printf(" = %d consumed\n", total);

  /* This is not really needed, but will be helpful for making sure that you
   * see output prior to a segmentation vioilation.  This is not usually a
   * good practice as we want to avoid ending the CPU burst premaurely which
   * this will do, but it is a helpful technique.
   */
  fflush(stdout);
  sem_post(&sem_print); // release the semaphore for printing
};

/**
 * @brief   Show how many requests of each type produced.
 *          Show how many requests consumed by each consumer.
 *
 * @param produced   count for each RequestType produced
 * @param consumed   array of pointers to consumed arrays for each consumer
 *                   e.g. consumed[TX] points to an array that
 *                   is indexed by request type
 *                   (it is an 2-D array, consumed[TX][GeneralTable] is
 *                    the number for General Table requests that were
 *                    consumered or served by T-X robot)
 */
void output_production_history(unsigned int produced[],
                               unsigned int *consumed[]) {
  int p, c;  /* array indices */
  int total;

  printf("\nREQUEST REPORT\n----------------------------------------\n");

  /* show number produced for each producer / request type */
  for (p = 0; p < RequestTypeN; p++) {
    printf("%s producer generated %d requests\n",
	   producerNames[p], produced[p]);
  }

  /* show number consumed by each consumer */
  for (c=0; c < ConsumerTypeN; c++) {
    printf("%s consumed ", consumerNames[c]);
    total = 0;
    for (p = 0; p < RequestTypeN; p++) {
      if (p > 0)
	printf(" + ");
      total += consumed[c][p];
      printf("%d %s", consumed[c][p], producerAbbrevs[p]);
    }
    printf(" = %d total\n", total);
  }

  printf("Elapsed time %.3f s\n", elapsed_s());
}
