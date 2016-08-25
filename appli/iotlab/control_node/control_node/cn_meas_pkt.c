#include <string.h>
#include "cn_meas_pkt.h"

enum {
    NUM_PKT_OFFSET = 0,
    TIME_OFFSET = 1,
    SEC = 1000 * 1000,
};


static void _pkt_inc_measure_count(iotlab_packet_t *packet);
static void _pkt_add_measure_time(iotlab_packet_t *packet,
                                  struct soft_timer_timeval *timestamp);
static int _pkt_should_send(iotlab_packet_t *packet, size_t measure_size,
                            struct soft_timer_timeval *timestamp);

static uint32_t _us_since_packet_ref(iotlab_packet_t *packet,
                                     struct soft_timer_timeval *timestamp);


iotlab_packet_t *cn_meas_pkt_lazy_alloc(
        iotlab_packet_queue_t *queue,
        iotlab_packet_t *current_packet,
        struct soft_timer_timeval *timestamp)
{
    iotlab_packet_t *packet;

    if (current_packet)
        return current_packet;

    packet = iotlab_serial_packet_alloc(queue);
    if (NULL == packet)
        return NULL;

    /* init new measure packet
     * + measure count
     * + timestamp seconds
     */
    ((packet_t *)packet)->data[NUM_PKT_OFFSET] = 0;  // empty packet
    ((packet_t *)packet)->length  = 1;  // measures number byte
    iotlab_packet_append_data(packet, &timestamp->tv_sec, sizeof(uint32_t));

    return packet;
}

int cn_meas_pkt_add_measure(
        iotlab_packet_t *packet,
        struct soft_timer_timeval *timestamp,
        size_t measure_size,
        struct cn_meas *measures)
{
    _pkt_inc_measure_count(packet);
    _pkt_add_measure_time(packet, timestamp);

    struct cn_meas *meas = NULL;
    for (meas = measures; meas->addr != NULL; (meas = (++measures)))
        iotlab_packet_append_data(packet, meas->addr, meas->size);

    return _pkt_should_send(packet, measure_size, timestamp);
}

void cn_meas_pkt_flush(iotlab_packet_t **packet_p, uint8_t type)
{
    iotlab_packet_t* packet = *packet_p;

    if (NULL == packet)
        return;
    if (iotlab_serial_send_frame(type, packet))
        iotlab_packet_call_free(packet);  // send fail

    *packet_p = NULL;
}


/*
 * Packet manipulation
 */
static void _pkt_inc_measure_count(iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    pkt->data[NUM_PKT_OFFSET]++;
}

/* Store the number of µs since packet time reference */
static void _pkt_add_measure_time(iotlab_packet_t *packet,
                                 struct soft_timer_timeval *timestamp)
{
    uint32_t usecs = _us_since_packet_ref(packet, timestamp);
    iotlab_packet_append_data(packet, &usecs, sizeof(uint32_t));
}

static int _pkt_should_send(iotlab_packet_t *packet, size_t measure_size,
                            struct soft_timer_timeval *timestamp)
{

    uint32_t usecs = _us_since_packet_ref(packet, timestamp);

    /* Packet full */
    if (iotlab_serial_packet_free_space(packet) < measure_size)
        return 1;

    /* No packet has been sent for a long time:
     * around one or two seconds */
    if (usecs > (2 * SEC))
        return 1;

    return 0;
}

static uint32_t _us_since_packet_ref(iotlab_packet_t *packet,
                                     struct soft_timer_timeval *timestamp)
{
    packet_t *pkt = (packet_t *)packet;

    // Read t_ref from packet
    uint32_t t_ref_s;
    memcpy(&t_ref_s, &pkt->data[TIME_OFFSET], sizeof(t_ref_s));

    /* Get number of µs since t_ref_s */
    uint32_t usecs;
    usecs = timestamp->tv_usec;
    usecs += (timestamp->tv_sec - t_ref_s) * SEC;

    return usecs;
}
