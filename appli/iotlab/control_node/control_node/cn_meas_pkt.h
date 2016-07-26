#ifndef CN_MEAS_PKT_H
#define CN_MEAS_PKT_H
#include <stddef.h>
#include "iotlab_serial.h"

/*
 * Control node measures packets content have the following format
 *
 * Bytes:
 *
 * - uint8_t:   Num measures
 * - uint32_t:  Base timestamp in seconds
 *
 * - measure:   Measure 1
 * - measure:   Measure 2
 * - ...
 * - measure:   Measure 'Num measures'
 *
 *
 * Per measure
 * - uint32_t: Timestamp diff in microseconds:
 *             Measure timestamp == Base timestamp s + timestamp diff us
 * - measure_size - uint32_t:  Packed measure
 *     + measure data 0
 *     + measure data 1
 *     + measure data 2
 *
 */

struct cn_meas {
    void *addr;
    size_t size;
};

/**
 * Alloc and initialize a new packet in needed.
 * Packet is initialized with 'num measures' == 0 and base timestamp stored.
 *
 * \param queue          queue where to alloc packet from
 * \param current_packet Decide if a new one should be alloc in NULL
 * \param timestamp      base timestamp to store in packet
 *
 * \return current_packet if not NULL or a new initialized packet
 */
iotlab_packet_t *cn_meas_pkt_lazy_alloc(iotlab_packet_queue_t *queue,
                                        iotlab_packet_t *current_packet,
                                        struct soft_timer_timeval *timestamp);

/**
 * Add a new measure to packet with
 *     timestamp_us + [measure|measure2|measure3]
 *
 * \param packet       measure packet to use
 * \param timestamp    measure timestamp
 * \param measure_size timestamp + measure size == 4 + sizeof(measures)
 * \param measures     list of measures, should be {NULL, 0} terminated
 *
 * \return true if packet should be sent. It should be sent if:
 *           Packet is full == cannot contain another measure
 *           timestamp - ref_timestamp_s > 2 seconds
 */
int cn_meas_pkt_add_measure(iotlab_packet_t *packet,
                            struct soft_timer_timeval *timestamp,
                            size_t measure_size,
                            struct cn_meas *measures);

/**
 * Send pointed packet with type. If sending fails, packet is freed directly.
 * Packet pointer is set to NULL.
 *
 * \param packet_p pointer to packet to send, it will be set to NULL.
 * \param type     packet data type
 */
void cn_meas_pkt_flush(iotlab_packet_t **packet_p, uint8_t type);

#endif//CN_MEAS_PKT_H
