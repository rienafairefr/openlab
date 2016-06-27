#ifndef EVENT_CUSTOM_H
#define EVENT_CUSTOM_H
#include "task.h"
#include "queue.h"

// typedef
typedef struct
{
    handler_t event;
    handler_arg_t event_arg;
} queue_entry_t;


struct event_queue {
    // Configure this
    unsigned priority;
    unsigned short stack_size;
    unsigned queue_length;

    // Internal variables
    xTaskHandle task;
    xQueueHandle queue;
    queue_entry_t current_event;
    unsigned num;
};

/**
 * Advanced initialization of the event mechanism.
 * To be used instead of regular event_init.
 *
 * This creates tasks according to event_queues configuration
 *
 * \param event_queues array of queues with priority/stack_size/queue_length
 * configuration
 *     If stack_size is 0 use configMINIMAL_STACK_SIZE
 *     If queue_length is 0 use EVENT_QUEUE_LENGTH
 * \param num number of elements in event_queues
 */
void event_init_with_queues(struct event_queue *event_queues, int num);

#endif//EVENT_CUSTOM_H
