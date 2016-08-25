#include "platform.h"

#include <string.h>

#include "printf.h"
#include "debug.h"


// Mock uart_transfer_async
static void uart_transfer_async_mock(uart_t uart,
        const uint8_t *tx_buffer, uint16_t length,
        handler_t handler, handler_arg_t handler_arg);
#define uart_transfer_async uart_transfer_async_mock
// Include C file for tests
#include "iotlab_serial.c"


#define ASSERT(exp)\
    do {\
        if(!(exp))\
        {\
            log_error("ASSERT(%s) failed at line %d", #exp, __LINE__);\
        }\
    }\
    while(0);


static void send_test_packet(uint8_t cmd_type, uint8_t *data, uint8_t size);

static uint32_t uart_transfer_async_call_count = 0;
static uint8_t uart_transfer_async_pkt[300];
static uint8_t uart_transfer_async_length = 0;

static void uart_transfer_async_mock(uart_t uart,
        const uint8_t *tx_buffer, uint16_t length,
        handler_t handler, handler_arg_t handler_arg)
{

    uart_transfer_async_call_count++;
    (void)uart;

    uart_transfer_async_length = length;
    memcpy(uart_transfer_async_pkt, tx_buffer, uart_transfer_async_length);

    // 500000 bauds =~ 16us per byte
    static soft_timer_t end_of_tx_timer;
    uint32_t delay = soft_timer_us_to_ticks(
            16 * ((uint32_t)length + 1)); // round up

    soft_timer_set_handler(&end_of_tx_timer, handler, handler_arg);
    soft_timer_set_event_priority(&end_of_tx_timer, EVENT_QUEUE_NETWORK);
    soft_timer_start(&end_of_tx_timer, delay, 0);
}

static int32_t first_handler(uint8_t cmd_type, iotlab_packet_t *pkt);
static uint32_t first_call_count = 0;
static int32_t first_ret = 0;
static int32_t second_handler(uint8_t cmd_type, iotlab_packet_t *pkt);
static uint32_t second_call_count = 0;
static int32_t second_ret = 0;
static int32_t measures_handler(uint8_t cmd_type, iotlab_packet_t *pkt);

static iotlab_serial_handler_t first = {
    .cmd_type = 0x01,
    .handler = first_handler,
    .next = NULL,
};

static iotlab_serial_handler_t second = {
    .cmd_type = 0x02,
    .handler = second_handler,
    .next = NULL,
};
static iotlab_serial_handler_t measures = {
    .cmd_type = 0x05,
    .handler = measures_handler,
    .next = NULL,
};


iotlab_packet_queue_t tx_packets;

#define NUM_PKTS 10
iotlab_packet_t tx_pkts[NUM_PKTS];

static void init(void)
{
    iotlab_serial_start(500000);

    iotlab_packet_init_queue(&tx_packets, tx_pkts, NUM_PKTS);
    iotlab_serial_register_handler(&first);
    iotlab_serial_register_handler(&second);
    iotlab_serial_register_handler(&measures);
}

static int32_t first_handler(uint8_t cmd_type, iotlab_packet_t *pkt)
{
    ASSERT(cmd_type == first.cmd_type);
    first_call_count++;
    return first_ret;
}

static int32_t second_handler(uint8_t cmd_type, iotlab_packet_t *pkt)
{
    ASSERT(cmd_type == second.cmd_type);
    second_call_count++;
    return second_ret;
}


static void test_handlers()
{
    /* Test one handler with different configurations */
    send_test_packet(first.cmd_type, (uint8_t *)"first", sizeof("first"));
    ASSERT(first_call_count == 1);
    ASSERT(second_call_count == 0);
    ASSERT(uart_transfer_async_call_count == 1);
    ASSERT(uart_transfer_async_length == (IOTLAB_SERIAL_HEADER_SIZE + 1));
    ASSERT(uart_transfer_async_pkt[IOTLAB_SERIAL_HEADER_SIZE + 1 -1] == ACK);

    // Check full packet
    ASSERT(uart_transfer_async_pkt[0] == SYNC_BYTE);
    ASSERT(uart_transfer_async_pkt[1] == 2);
    ASSERT(uart_transfer_async_pkt[2] == first.cmd_type);
    ASSERT(uart_transfer_async_pkt[3] == ACK);

    first_ret = 42;
    send_test_packet(first.cmd_type, (uint8_t *)"first", sizeof("first"));
    ASSERT(first_call_count == 2);
    ASSERT(second_call_count == 0);
    ASSERT(uart_transfer_async_call_count == 2);
    ASSERT(uart_transfer_async_length == (IOTLAB_SERIAL_HEADER_SIZE + 1));
    ASSERT(uart_transfer_async_pkt[IOTLAB_SERIAL_HEADER_SIZE + 1 -1] == NACK);

    first_ret = 0;
    send_test_packet(first.cmd_type, (uint8_t *)"first", sizeof("first"));
    ASSERT(first_call_count == 3);
    ASSERT(second_call_count == 0);
    ASSERT(uart_transfer_async_call_count == 3);
    ASSERT(uart_transfer_async_length == (IOTLAB_SERIAL_HEADER_SIZE + 1));
    ASSERT(uart_transfer_async_pkt[IOTLAB_SERIAL_HEADER_SIZE + 1 -1] == ACK);

    uart_transfer_async_call_count = 0;

    /* Test other handlers */
    send_test_packet(second.cmd_type, (uint8_t *)"second", sizeof("second"));
    ASSERT(first_call_count == 3);
    ASSERT(second_call_count == 1);
    ASSERT(uart_transfer_async_call_count == 1);
    ASSERT(uart_transfer_async_length == (IOTLAB_SERIAL_HEADER_SIZE + 1));
    ASSERT(uart_transfer_async_pkt[IOTLAB_SERIAL_HEADER_SIZE + 1 -1] == ACK);

    // Invalid handler
    send_test_packet(0x03, (uint8_t *)"third", sizeof("third"));
    ASSERT(first_call_count == 3);
    ASSERT(second_call_count == 1);
    ASSERT(uart_transfer_async_call_count == 2);
    ASSERT(uart_transfer_async_length == (IOTLAB_SERIAL_HEADER_SIZE + 1));
    ASSERT(uart_transfer_async_pkt[IOTLAB_SERIAL_HEADER_SIZE + 1 -1] == NACK);
    ASSERT(uart_transfer_async_pkt[2] == 0x03);
}

static int measures_active = 0;
static uint32_t measures_count = 0;
static uint32_t measures_event_call_count = 0;

static uint8_t ref_data[IOTLAB_SERIAL_DATA_MAX_SIZE] = {0xFF};


static void measure_event(handler_arg_t arg)
{
    if (!measures_active)
        return;
    if (event_post(EVENT_QUEUE_APPLI, measure_event, NULL))
        ASSERT(0);

    measures_event_call_count++;

    iotlab_packet_t *packet = iotlab_serial_packet_alloc(&tx_packets);
    if (packet == NULL)
        return;

    measures_count++;

    iotlab_packet_append_data(packet, ref_data, sizeof(ref_data));
    ASSERT(iotlab_serial_packet_free_space(packet) == 0);

    iotlab_serial_send_frame(0xF0 | measures.cmd_type, packet);
}

static void config_measures(handler_arg_t arg)
{
    uint8_t mode = (uint8_t)(uint32_t)arg;
    send_test_packet(measures.cmd_type, &mode, sizeof(mode));
}


static uint32_t measures_handler_pkt_free_count = 0;
static void measures_handler_pkt_free(iotlab_packet_t *packet)
{
    measures_handler_pkt_free_count++;
    iotlab_packet_free(packet);
}


static int32_t measures_handler(uint8_t cmd_type, iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    ASSERT(cmd_type == measures.cmd_type);
    ASSERT(pkt->length == 1);

    // 'free' callback not used by iotlab_serial, may change in the future
    // will need to use another test method
    // (like calling it also in our 'free' function)
    ASSERT(packet->free == iotlab_packet_free);

    packet->free = measures_handler_pkt_free;

    if (pkt->data[0]) {
        measures_active = 1;
        event_post(EVENT_QUEUE_APPLI, measure_event, NULL);
    } else {
        measures_active = 0;
    }
    return 0;
}

#define DELAY_BEFORE_STOP_S (5)
#define DELAY_BEFORE_STOP soft_timer_s_to_ticks(DELAY_BEFORE_STOP_S)

static void test_measures()
{
    static soft_timer_t timer;
    uint32_t t0 = soft_timer_time();

    uart_transfer_async_call_count = 0;

    // Schedule stop after some delay
    soft_timer_set_handler(&timer, config_measures, (handler_arg_t)0);
    soft_timer_set_event_priority(&timer, EVENT_QUEUE_NETWORK);
    soft_timer_start(&timer, DELAY_BEFORE_STOP, 0);

    ASSERT(measures_active == 0);
    event_post(EVENT_QUEUE_NETWORK, config_measures, (handler_arg_t)1);

    // OS should not give back time to this task before the end of the measures
    // because we have almost lowest priority and it should be busy calling
    // measures_handler
    ASSERT(soft_timer_time() - t0 >= DELAY_BEFORE_STOP);
    ASSERT(measures_active == 0);

    // wait current packet transmitted and measure packet also, + margin
    soft_timer_delay_us(16 * (256 + 4) + 100);

    // All commands have been received
    ASSERT(measures_handler_pkt_free_count == 2);
    // There are remaining measures
    ASSERT(iotlab_packet_fifo_count(&tx_packets) != NUM_PKTS);

    // Wait a bit more for remaining messages to be sent
    soft_timer_delay_us(16 * (256) * NUM_PKTS);
    ASSERT(iotlab_packet_fifo_count(&tx_packets) == NUM_PKTS);

    // Two packets for config and the other for measures
    ASSERT(2 + measures_count == uart_transfer_async_call_count);

    ASSERT(measures_event_call_count > measures_count);

    ASSERT(measures_count > (DELAY_BEFORE_STOP_S * 400));
    ASSERT(measures_event_call_count > DELAY_BEFORE_STOP_S * 100000);
}

static void test_app(void *arg)
{
    (void)arg;
    init();
    log_info("Running tests");
    test_handlers();
    test_measures();
    log_info("Tests finished");

    vTaskDelete(NULL);
}

int main()
{
    platform_init();
    soft_timer_init();

    // Create a task for the application
    xTaskCreate(test_app, (signed char *) "test_iotlab_serial",
            4 * configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    platform_run();

    return 0;
}


static void send_test_packet(uint8_t cmd_type, uint8_t *data, uint8_t size)
{
    int i;
    ASSERT(size < 255);

    char_rx(NULL, SYNC_BYTE);
    char_rx(NULL, size + 1);  // type byte
    char_rx(NULL, cmd_type);

    for (i = 0; i < size; i++)
        char_rx(NULL, data[i]);
}
