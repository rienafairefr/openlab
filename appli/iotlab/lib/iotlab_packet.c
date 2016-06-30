#include <string.h>
#include "iotlab_packet.h"


#define LOG_LEVEL LOG_LEVEL_INFO
#include "debug.h"


#define LIST_PUT(prev, packet) do {      \
        (packet)->next = (prev)->next;   \
        (prev)->next   = (packet);       \
} while (0)
#define LIST_GET(prev, packet) do {          \
        (packet) = (prev)->next;             \
        (prev)->next = (prev)->next->next;   \
} while (0)

static void iotlab_packet_reset(iotlab_packet_t *packet);


void iotlab_packet_init_queue(iotlab_packet_queue_t *queue,
        iotlab_packet_t *packets, int size)
{
    int i;
    queue->count = 0;
    queue->head.next = NULL;

    queue->mutex = xSemaphoreCreateMutex();
    queue->new_packet = xSemaphoreCreateMutex();

    for (i = 0; i < size; i++) {
        iotlab_packet_t *packet = &packets[i];
        packet->storage = queue;
        iotlab_packet_free(packet);
    }
}


void iotlab_packet_free(iotlab_packet_t *packet)
{
    iotlab_packet_queue_t *queue = packet->storage;
    if (queue == NULL) {
        log_error("packet storage is null: %x -> storage", packet);
        return;
    }

    iotlab_packet_reset(packet);
    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    {
        LIST_PUT(&(queue->head), (packet_t *)packet);
        queue->count++;
    }
    xSemaphoreGive(queue->mutex);
}


iotlab_packet_t *iotlab_packet_alloc(iotlab_packet_queue_t *queue,
        uint8_t offset)
{
    packet_t *packet = NULL;
    if (queue->count == 0)
        return NULL;

    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    {
        LIST_GET(&(queue->head), packet);
        --queue->count;
    }
    xSemaphoreGive(queue->mutex);

    packet_reset(packet, offset);
    return (iotlab_packet_t *)packet;
}


static void iotlab_packet_reset(iotlab_packet_t *packet)
{
    packet_reset((packet_t *)packet, 0);
    packet->free = iotlab_packet_free;
    packet->timestamp = 0;
}


void iotlab_packet_append_data(iotlab_packet_t *packet, void *data, size_t size)
{
    packet_t *pkt = (packet_t *)packet;
    memcpy(&pkt->data[pkt->length], data, size);
    pkt->length += size;
}


int iotlab_packet_fifo_count(iotlab_packet_queue_t *queue)
{
    return queue->count;
}

#define DEBUG_VALUES_AND_PRIO(prev, packet) do {                  \
    log_debug("prev: %x->%x : %s", prev, prev->next, prev->data); \
    log_debug("next: %s", prev->next->data);                      \
    log_debug("prio: %d next(%d)", packet->priority,              \
            ((iotlab_packet_t *)prev->next)->priority);           \
} while (0)


void iotlab_packet_fifo_prio_append(iotlab_packet_queue_t *queue,
        iotlab_packet_t *packet)
{
    log_debug("insert: %x->%x : %s", packet, packet->pkt.next, packet->pkt.data);
    packet_t *prev = NULL;

    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    {
        for (prev = &queue->head; prev->next != NULL; prev = prev->next) {
            DEBUG_VALUES_AND_PRIO(prev, packet);
            if (packet->priority > ((iotlab_packet_t *)prev->next)->priority)
                break;

            // Not last one and prio is lower or equal to the next packet
        }
        log_debug("out_loop: prev: %x->%x", prev, prev->next);
        LIST_PUT(prev, (packet_t *)packet);

        if (queue->count++ == 0)
            xSemaphoreGive(queue->new_packet);
    }
    xSemaphoreGive(queue->mutex);
}

iotlab_packet_t *iotlab_packet_fifo_get(iotlab_packet_queue_t *queue)
{
    packet_t *packet = NULL;

    xSemaphoreTake(queue->mutex, portMAX_DELAY);
    {
        // Wait with mutex, in a while loop as another consumer may have taken
        // it
        while (queue->count == 0) {
            xSemaphoreGive(queue->mutex);
            xSemaphoreTake(queue->new_packet, portMAX_DELAY);
            xSemaphoreTake(queue->mutex, portMAX_DELAY);
        }

        LIST_GET(&(queue->head), packet);
        --queue->count;

    }
    xSemaphoreGive(queue->mutex);

    packet->next = NULL;

    return (iotlab_packet_t *)packet;
}
