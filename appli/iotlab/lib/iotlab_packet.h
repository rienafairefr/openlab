#ifndef IOTLAB_PACKET_H_
#define IOTLAB_PACKET_H_

#include "FreeRTOS.h"
#include "semphr.h"

#include "packet.h"
#include "soft_timer.h"

struct iotlab_packet_queue;

typedef struct iotlab_packet
{
    packet_t p;

    /** Start of transmit and reception timestamp */
    uint32_t timestamp;

    /** Function to free given packet*/
    void (*free)(struct iotlab_packet *);

    unsigned priority;

    struct iotlab_packet_queue *storage;
} iotlab_packet_t;


/** Macro to call packet free function
 * \param pkt a pointer to a iotlab_packet_t
 */
#define iotlab_packet_call_free(pkt) ((pkt)->free((pkt)))

typedef struct iotlab_packet_queue
{
    int count;
    packet_t head;
    xSemaphoreHandle mutex;
    xSemaphoreHandle new_packet;
} iotlab_packet_queue_t;



void iotlab_packet_init_queue(iotlab_packet_queue_t *queue,
        iotlab_packet_t *packets, int size);


/**
 * Append a packet to a packet FIFO.
 * \param fifo a pointer to the FIFO;
 * \param packet the packet to append;
 */
void iotlab_packet_fifo_prio_append(iotlab_packet_queue_t *queue,
        iotlab_packet_t *packet);

int iotlab_packet_fifo_count(iotlab_packet_queue_t *queue);

iotlab_packet_t *iotlab_packet_fifo_get(iotlab_packet_queue_t *queue);

/**
 * Alloc packet
 */
iotlab_packet_t *iotlab_packet_alloc(iotlab_packet_queue_t *queue,
       uint8_t offset);

/**
 * Free packet
 */
void iotlab_packet_free(iotlab_packet_t *packet);


/**
 * Append data in the serial packet
 */
void iotlab_packet_append_data(iotlab_packet_t *packet, void *data, size_t size);

#endif//IOTLAB_PACKET_H_
