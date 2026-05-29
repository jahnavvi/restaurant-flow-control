#ifndef SEATING_H
#define SEATING_H

/*
 * Arrays with producer and consumer names
 * These can be indexed with the enumerated types below
 * and are defined in log.c
 */
extern const char *producerNames[];
extern const char *producerAbbrevs[];
extern const char *consumerNames[];

/*
 * Enumerated types to be used by the producers and consumers
 * These are expected in the input/output functions (log.h)
 * should be useful in your producer and consumer code.
 */

/**
 * The queue can hold up to a maximum of 18 unconsumed requests
 * in its request queue at any given time.
 * e.	there can be no more than 6 VIP room requests
 * in the request queue at any given time.
*/

/*
 * Two greeter robots are used for producing requests:
 *   one for receiving and inserting a general table request,
 *   one for receiving and inserting a VIP request.
 * Each producer only deals with one type of requests,
 * so RequestType is synomonous with the producer type
 */
typedef enum Requests {
    GeneralTable = 0,   // General table request
    VIPRoom = 1,        // VIP room request
    RequestTypeN = 2,   // number of trade request types
  } RequestType;

/*
 * Two concierge robots are used for consuming requests:
 *   Requests are taken off from the request queue (by consumers)
 *   in the order that they are produced and inserted.
*/
typedef enum Consumers {
    TX = 0,     // T-X robot
    Rev9 = 1,   // Rev 9 robot
    ConsumerTypeN = 2, // Number of consumers
  } ConsumerType;

#endif
