/*
 * This file is part of HiKoB Openlab.
 *
 * HiKoB Openlab is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, version 3.
 *
 * HiKoB Openlab is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with HiKoB Openlab. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2013 HiKoB.
 */

/*
 * iotlab-serial.c
 *
 *  Created on: Aug 12, 2013
 *      Author: burindes
 */
#include "platform.h"
#include "iotlab_leds.h"
#include "iotlab_serial.h"

#include "debug.h"
#include "event.h"

#if defined(RELEASE) && RELEASE
#define ASYNCHRONOUS 1
#else // ASYNCHRONOUS
#define ASYNCHRONOUS 0
#endif // ASYNCHRONOUS

enum
{
    SYNC_BYTE = 0x80,

    ACK = 0x0A,
    NACK = 0x02
};

/** Handler for IDLE check */
static int32_t check_uart(handler_arg_t arg);
/** Handler for character received */
static void char_rx(handler_arg_t arg, uint8_t c);

static void allocate_rx_packet(handler_arg_t arg);
static void packet_received(handler_arg_t arg);
static void send_now(handler_arg_t arg);

#if ASYNCHRONOUS
/** Function called at the end of a UART TX transfer */
static void tx_done_isr(handler_arg_t arg);
#endif // ASYNCHRONOUS


// Two should be enough for commands,
#define IOTLAB_SERIAL_NUM_RX_PKTS (2)


/** */
static void handle_packet_sent(handler_arg_t arg);

static struct {
    iotlab_serial_handler_t *first_handler;

    /** Structure holding the TX information */
    struct {
        /** The FIFO of packets to send */
        iotlab_packet_queue_t fifo;

        /** The packet in TX. NULL if idle */
        iotlab_packet_t *pkt;

        /** Flag to indicate end of asynchronous TX */
        volatile uint32_t irq_triggered;
    } tx;

    /** Structure holding RX information */
    struct {
        /** The packet being received */
        iotlab_packet_t * volatile rx_pkt;

        /** The complete received packet */
        iotlab_packet_t * volatile ready_pkt;

        iotlab_packet_t packets[IOTLAB_SERIAL_NUM_RX_PKTS];
        iotlab_packet_queue_t queue;
    } rx;

    uint8_t logger_type_byte;
} ser;


void iotlab_serial_start(uint32_t baudrate)
{
    uart_enable(uart_external, baudrate);
    iotlab_packet_init_queue(&ser.rx.queue,
            ser.rx.packets, IOTLAB_SERIAL_NUM_RX_PKTS);

    // Clear the first handler
    ser.first_handler = NULL;

    // Clear RX/TX structures
    iotlab_packet_init_queue(&ser.tx.fifo, NULL, 0);
    ser.tx.pkt = NULL;
    ser.tx.irq_triggered = 0;

    ser.rx.rx_pkt = NULL;
    ser.rx.ready_pkt = NULL;

    // Configure serial port
    uart_set_rx_handler(uart_external, char_rx, NULL);

    // Set the UART priority higher than FreeRTOS limit, to receive all chars.
    // But be careful, the interrupt handler CANNOT use FreeRTOS or event functions
    // Therefore we use the platform IDLE handler to check for input
    uart_set_irq_priority(uart_external, 0x10);
    platform_set_idle_handler(check_uart, NULL);

    // allocate first packet
    allocate_rx_packet(NULL);
}

void iotlab_serial_register_handler(iotlab_serial_handler_t *handler)
{
    // Insert on head
    handler->next = ser.first_handler;
    ser.first_handler = handler;
}

static int32_t iotlab_serial_init_tx_packet(uint8_t type,
        iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    if (pkt->length > _PAYLOAD_MAX) {
        leds_on(RED_LED);
        return 1;  // pkt too long
    } else if (pkt->data - pkt->raw_data < IOTLAB_SERIAL_HEADER_SIZE) {
        leds_on(RED_LED);
        return 2;  // Header not respected
    }

    // Set header
    pkt->raw_data[0] = SYNC_BYTE;
    pkt->raw_data[1] = pkt->length + 1;  // for type byte
    pkt->raw_data[2] = type;
    pkt->length += IOTLAB_SERIAL_HEADER_SIZE;

    return 0;
}

int32_t iotlab_serial_send_frame(uint8_t type, iotlab_packet_t *pkt)
{
    int32_t ret = iotlab_serial_init_tx_packet(type, pkt);
    if (ret)
        return ret;

    iotlab_packet_fifo_prio_append(&ser.tx.fifo, pkt);

    send_now(NULL);
    return 0;
}

iotlab_packet_t *iotlab_serial_packet_alloc(iotlab_packet_queue_t *queue)
{
    return iotlab_packet_alloc(queue, IOTLAB_SERIAL_HEADER_SIZE);
}


int32_t iotlab_serial_packet_free_space(iotlab_packet_t *packet)
{
    return IOTLAB_SERIAL_DATA_MAX_SIZE - ((packet_t *)packet)->length;
}

static void char_rx(handler_arg_t arg, uint8_t c)
{
    packet_t *pkt;
    /*
     * HIGH PRIORITY Interrupt
     *
     * Do not use FreeRTOS or Event library functions!!!
     */
    static uint16_t rx_index = 0;
    static uint32_t last_start_time = 0;
    uint32_t timestamp = soft_timer_time();

    // Check for ready buffer
    if (ser.rx.rx_pkt == NULL)
    {
        // Request allocation
        rx_index = 0;
        return;
    }
    pkt = (packet_t *)ser.rx.rx_pkt;

    // Check if packet started too long ago
    if (last_start_time
            && (timestamp - last_start_time
                > soft_timer_ms_to_ticks(100)))
    {
        // Reset index
        rx_index = 0;
        last_start_time = 0;
    }

    // A char is received, switch index
    switch (rx_index) {
        case 0:
            // the received char should be a start
            if (c != SYNC_BYTE)
                return;  // Abort
            // Store time
            last_start_time = timestamp;

            // Rx start timestamp
            ser.rx.rx_pkt->timestamp = timestamp;
            break;
        case 1:
            // length byte
            pkt->length = 2 + c;
            break;
        default:
            // Proceed
            break;
    }

    // Save byte
    pkt->raw_data[rx_index] = c;

    // Increment
    rx_index++;

    if (rx_index < 2)
        return;

    // Check length
    if (rx_index == pkt->length) {
        // Reset index
        rx_index = 0;
        last_start_time = 0;

        // Switch buffers
        ser.rx.ready_pkt = ser.rx.rx_pkt;
        ser.rx.rx_pkt = NULL;
    }
}

static int32_t check_uart(handler_arg_t arg)
{
    int32_t ret = 0;

    if (ser.rx.ready_pkt) {
        event_post(EVENT_QUEUE_APPLI, packet_received, NULL);
        ret = 1;
    }

    if (ser.rx.rx_pkt == NULL) {
        event_post(EVENT_QUEUE_APPLI, allocate_rx_packet, NULL);
        ret = 1;
    }

    if (ser.tx.irq_triggered) {
        event_post(EVENT_QUEUE_APPLI, handle_packet_sent, NULL);
        ret = 1;
    }

    return ret;
}

static void allocate_rx_packet(handler_arg_t arg)
{
    // Allocate a new packet for RX
    if (ser.rx.rx_pkt == NULL)
        ser.rx.rx_pkt = iotlab_serial_packet_alloc(&ser.rx.queue);
}

static void packet_received(handler_arg_t arg)
{
    packet_t *rx_pkt;

    // Allocate a new packet for RX
    allocate_rx_packet(NULL);

    // Get the ready packet
    rx_pkt = (packet_t *)ser.rx.ready_pkt;

    // Prepare packets for next RX
    ser.rx.ready_pkt = NULL;

    // Get the command type header
    uint8_t cmd_type = rx_pkt->raw_data[2];

    // Loop over the registered handlers to find a match
    iotlab_serial_handler_t *handler = ser.first_handler;

    int32_t result = 1;

    // Remove header
    rx_pkt->length -= IOTLAB_SERIAL_HEADER_SIZE;

    while (handler != NULL) {
        if (handler->cmd_type == cmd_type) {
            // Found! process
            result = handler->handler(cmd_type, (iotlab_packet_t *)rx_pkt);
            break;
        }
        // Increment
        handler = handler->next;
    }
    if (NULL == handler)
        result = 1;

    rx_pkt->length = 1;
    rx_pkt->data[0] = ((0 == result) ? ACK : NACK);

    iotlab_serial_send_frame(cmd_type, (iotlab_packet_t *)rx_pkt);
    // auto clean of packets
}

/* Start sending packets if lib was idle */
static void send_now(handler_arg_t arg)
{
    packet_t *pkt;
    if (ser.tx.pkt)
        return;  // Uart Send active, tx.fifo will be handled asyncronously

    // Try to get a frame from the FIFO
    if (iotlab_packet_fifo_count(&ser.tx.fifo) == 0)
        return;  // nothing to send

    // Only one reader so it's safe, it won't block
    ser.tx.pkt = iotlab_packet_fifo_get(&ser.tx.fifo);

    pkt = (packet_t *)ser.tx.pkt;

    // Tx start timestamp
    ser.tx.pkt->timestamp = soft_timer_time();

    // Start sending the packet
#if ASYNCHRONOUS
    uart_transfer_async(uart_external, pkt->raw_data, pkt->length,
            tx_done_isr, NULL);
#else // ASYNCHRONOUS
    uart_transfer(uart_external, pkt->raw_data, pkt->length);
    event_post(EVENT_QUEUE_APPLI, handle_packet_sent, NULL);
#endif // ASYNCHRONOUS
}

#if ASYNCHRONOUS
static void tx_done_isr(handler_arg_t arg)
{
    ser.tx.irq_triggered = 1;
}
#endif // ASYNCHRONOUS

static void handle_packet_sent(handler_arg_t arg)
{
    // Check there is a packet being sent
    if (ser.tx.pkt == NULL) {
        leds_on(RED_LED);
        log_error("Packet sent but no packet!");
        return;
    }

    // Free the packet
    iotlab_packet_call_free(ser.tx.pkt);

    // Clear the TX busy flag
    ser.tx.pkt = NULL;
    ser.tx.irq_triggered = 0;

    send_now(NULL);
}
