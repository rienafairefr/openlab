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
#include "FreeRTOS.h"
#include "semphr.h"
#include "platform.h"

#include "nvic_.h"
#include "exti.h"
#include "cm3_nvic_registers.h"

#include "iotlab_leds.h"
#include "iotlab_serial.h"

#include "debug.h"
#include "event.h"

enum
{
    SYNC_BYTE = 0x80,

    ACK = 0x0A,
    NACK = 0x02
};

/** Handler for IDLE check */
static int32_t idle_hook_uart(handler_arg_t arg);
/** Handler for character received */
static void char_rx(handler_arg_t arg, uint8_t c);
/** Handler for second interrupt */
static void check_uart(handler_arg_t arg);
static void uart_low_priority_interrupt_init(handler_t handler);
static void uart_low_priority_interrupt_trigger();

static void allocate_rx_packet(handler_arg_t arg);
static void iotlab_serial_tx_task(void *param);
static void packet_received(handler_arg_t arg);
static void send_packet(iotlab_packet_t *packet);


/** Function called at the end of a UART TX transfer */
static void tx_done_isr(handler_arg_t arg);


// Two should be enough for commands,
#define IOTLAB_SERIAL_NUM_RX_PKTS (2)
#define IOTLAB_SERIAL_ANSWER_PRIORITY 0x80


/*
 * Uart low priority interruption
 * Using an EXTI_LINE to simulate software interrupt
 */

#define  NVIC_IOTLAB_SERIAL_IRQ_LINE  NVIC_IRQ_LINE_EXTI2
#define  NVIC_IOTLAB_SERIAL_EXTI_LINE EXTI_LINE_Px2


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

        /** Semaphore to wait for tx end*/
        xSemaphoreHandle tx_end_event;
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
    /* compatibility with HiKoB */
    if (uart_external == NULL)
        uart_external = uart_print;

    uart_low_priority_interrupt_init(check_uart);
    ser.tx.tx_end_event = xSemaphoreCreateCounting(1, 0);

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
    platform_set_idle_handler(idle_hook_uart, NULL);

    xTaskCreate(iotlab_serial_tx_task, (signed char *)"serial_tx",
            configMINIMAL_STACK_SIZE, NULL,
            configMAX_PRIORITIES - 1, NULL);

    // allocate first packet
    allocate_rx_packet(NULL);
}

void iotlab_serial_register_handler(iotlab_serial_handler_t *handler)
{
    // Insert on head
    handler->next = ser.first_handler;
    ser.first_handler = handler;
}

iotlab_serial_handler_t *iotlab_serial_get_handler(uint8_t cmd_type)
{
    iotlab_serial_handler_t *handler = NULL;
    for (handler = ser.first_handler;
            handler != NULL;
            handler = handler->next) {
        if (handler->cmd_type == cmd_type)
            break;
    }
    return handler;
}

int32_t iotlab_serial_call_handler(uint8_t cmd_type, iotlab_packet_t *packet)
{
    // Remove header
    ((packet_t *)packet)->length -= IOTLAB_SERIAL_HEADER_SIZE;

    iotlab_serial_handler_t *handler = iotlab_serial_get_handler(cmd_type);
    if (handler)
        return handler->handler(cmd_type, packet);
    else
        return 1;
}

void iotlab_serial_send_result(iotlab_packet_t *packet, uint8_t cmd_type,
        int32_t result)
{
    packet->priority = IOTLAB_SERIAL_ANSWER_PRIORITY;
    packet_t *pkt = (packet_t *)packet;

    pkt->length = 1;
    pkt->data[0] = ((0 == result) ? ACK : NACK);

    iotlab_serial_send_frame(cmd_type, packet);
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

        uart_low_priority_interrupt_trigger();
    }
}


static void packet_received(handler_arg_t arg)
{
    if (ser.rx.ready_pkt == NULL)
        return;

    // Get the ready packet and prepare for next RX
    iotlab_packet_t *rx_pkt = ser.rx.ready_pkt;
    ser.rx.ready_pkt = NULL;

    uint8_t cmd_type = ((packet_t *)rx_pkt)->raw_data[2];
    int32_t result = iotlab_serial_call_handler(cmd_type, rx_pkt);

    iotlab_serial_send_result(rx_pkt, cmd_type, result);
}

// Transmission

static void iotlab_serial_tx_task(void *param)
{
    for (;;) {
        iotlab_packet_t *packet;
        packet = iotlab_packet_fifo_get(&ser.tx.fifo);  // Blocking
        send_packet(packet);
        iotlab_packet_call_free(packet);
    }
}

static void send_packet(iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;

    packet->timestamp = soft_timer_time();

    uart_transfer_async(uart_external, pkt->raw_data, pkt->length,
            tx_done_isr, NULL);
    // Block until finished
    xSemaphoreTake(ser.tx.tx_end_event, portMAX_DELAY);
}


/* Interrupts and idle_hook */

/* normal priority to allow calling OS functions */
static void uart_low_priority_interrupt_init(handler_t handler)
{
    nvic_enable_interrupt_line(NVIC_IOTLAB_SERIAL_IRQ_LINE);
    nvic_set_priority(NVIC_IOTLAB_SERIAL_IRQ_LINE, 0xFE);
    exti_set_handler(NVIC_IOTLAB_SERIAL_EXTI_LINE, handler, NULL);
}

static void uart_low_priority_interrupt_trigger()
{
    *cm3_nvic_get_STIR() = NVIC_IOTLAB_SERIAL_IRQ_LINE;
}


/* High priority, no OS functions allowed */

static void tx_done_isr(handler_arg_t arg)
{
    ser.tx.irq_triggered = 1;
    uart_low_priority_interrupt_trigger();
}

static void allocate_rx_packet(handler_arg_t arg)
{
    // Allocate a new packet for RX
    if (ser.rx.rx_pkt == NULL)
        ser.rx.rx_pkt = iotlab_serial_packet_alloc(&ser.rx.queue);
}

static void check_uart(handler_arg_t arg)
{
    portBASE_TYPE yield;
    allocate_rx_packet(NULL);

    if (ser.rx.ready_pkt) {
        event_post_from_isr(EVENT_QUEUE_APPLI, packet_received, NULL);
    }

    if (ser.tx.irq_triggered) {
        ser.tx.irq_triggered = 0;
        xSemaphoreGiveFromISR(ser.tx.tx_end_event, &yield);
        if (yield) {
            portYIELD();
        }
    }
}

// Idle hook may find cases where 'check_uart' failed to post from isr
static int32_t idle_hook_uart(handler_arg_t arg)
{
    (void)arg;

    if (ser.rx.ready_pkt) {
        uart_low_priority_interrupt_trigger();
        return 1;
    }
    return 0;
}
